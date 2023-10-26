#include "AppleM.h"
#include "Registry.h"

#if defined(__APPLE__) && defined(__MACH__) && defined(__AARCH64EL__)

extern "C" {

/* IOReport is part of private framework. Headers can be found all over the internet. Types are partly guessed. */

#import <CoreFoundation/CoreFoundation.h>

enum {
	kIOReportIterOk,
	kIOReportIterFailed,
	kIOReportIterSkipped
};

struct IOReportSubscription;
typedef struct IOReportSubscription *IOReportSubscriptionRef;
typedef CFDictionaryRef IOReportSampleRef;
typedef CFDictionaryRef IOReportChannelRef;

typedef int (^ioreportiterateblock)(IOReportSampleRef ch);
extern void IOReportIterate(CFDictionaryRef samples, ioreportiterateblock);

extern IOReportSubscriptionRef IOReportCreateSubscription(void *, CFMutableDictionaryRef, CFMutableDictionaryRef *, uint64_t, CFTypeRef);

// these two more likely return CFDictionaryRef. But makes stuff easier for my shared_ptr
extern CFMutableDictionaryRef IOReportCreateSamples(IOReportSubscriptionRef subs, CFMutableDictionaryRef channels, CFTypeRef);
extern CFMutableDictionaryRef IOReportCreateSamplesDelta(CFDictionaryRef prev, CFDictionaryRef current, CFTypeRef a);

extern uint64_t IOReportSimpleGetIntegerValue(IOReportChannelRef ch, void *);

/* based on usage in pmset.c */

extern CFMutableDictionaryRef IOReportCopyChannelsInGroup(CFStringRef, CFStringRef, uint64_t, uint64_t, uint64_t);


extern CFStringRef IOReportChannelGetSubGroup(CFDictionaryRef);
extern CFStringRef IOReportChannelGetChannelName(CFDictionaryRef);


typedef int IOReportIterationResult;
extern IOReportIterationResult IOReportMergeChannels(CFDictionaryRef desiredChs, IOReportChannelRef ch, CFTypeRef a);

}

/***********************************************************************/

typedef std::shared_ptr<__CFDictionary> CFMutableDictionaryPtr;

template<typename CF_T>
std::shared_ptr<CF_T> cf_shared(CF_T *obj)
{
	return std::shared_ptr<CF_T>(obj, [](CF_T *o){ if (o) CFRelease(o); });
}

/***********************************************************************/

/* From inspection on macOS 12.7 I found the functions IOReportCopyChannelsInGroup, IOReportCopyAllChannels etc.
 * to return a CFMutableDictionaryRef{"QueryOpts": CFNumber[Ref](0), "IOReportChannels": CFArray[Ref]}
 * I gullibly assume I can simply concatenate the array lists of multiple channel dictionaries
 * This behavior will most likely break in the future.
 */

// returns {"QueryOpts": 0, "IOReportChannels": []}
static CFMutableDictionaryRef empty_channel_list(void)
{
	CFMutableDictionaryRef resultDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// resultDict["QueryOpts"] = 0
	int zeroInt = 0;
	CFNumberRef zeroValue = CFNumberCreate(NULL, kCFNumberIntType, &zeroInt);
	CFDictionarySetValue(resultDict, CFSTR("QueryOpts"), zeroValue);
	CFRelease(zeroValue);
	
	// resultDict["IOReportChannels"] = []
	CFMutableArrayRef ioReportChannels = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFDictionarySetValue(resultDict, CFSTR("IOReportChannels"), ioReportChannels);
	CFRelease(ioReportChannels);
	
	return resultDict;
}


// returns {"QueryOpts": 0, "IOReportChannels": [channel]}
// does not change ref-count of channel (handled by the CFArray)
static CFMutableDictionaryRef channel2channel_list(IOReportChannelRef channel)
{
	CFMutableDictionaryRef resultDict = empty_channel_list();

	CFMutableArrayRef ioReportChannels = (CFMutableArrayRef)CFDictionaryGetValue(resultDict, CFSTR("IOReportChannels"));
	CFArrayAppendValue(ioReportChannels, channel);
	
	return resultDict;
}

/***********************************************************************/


std::string cfstr2stdstring(CFStringRef cfstr)
{
	if (cfstr == nullptr) {
		return std::string();
	}
	
	CFIndex length = CFStringGetLength(cfstr);
	CFIndex maxSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8) + 1;
	std::vector<char> stringBytes(maxSize);
	
	if (CFStringGetCString(cfstr, stringBytes.data(), maxSize, kCFStringEncodingUTF8)) {
		return std::string(stringBytes.data());
	} else {
		return std::string();
	}
}

class M1nPointSharedDetail
{
public:
	static std::string buildCounterName(IOReportChannelRef channel)
	{
		// IOReportChannelGetSubGroup etc do not retain ownership.
		// The returned strings must not be released.
		std::string name =
			  cfstr2stdstring(IOReportChannelGetSubGroup(channel))
			+ ":"
			+ cfstr2stdstring(IOReportChannelGetChannelName(channel));
		
		std::replace(name.begin(), name.end(), ' ', '_');
		return name;
	}

	M1nPointSharedDetail()
	{ }

	~M1nPointSharedDetail()
	{
		for (auto & name_ref: counter_to_channel) {
			if (name_ref.second)
				CFRelease(name_ref.second);
		}
	}

	CFMutableDictionaryPtr desired_channels;
	std::map<std::string, uint64_t> last_values;

	std::shared_ptr<IOReportSubscription> active_subscription;
	CFMutableDictionaryPtr subscribed_channels;
	CFMutableDictionaryPtr initial_samples;

	std::map<std::string, IOReportChannelRef> counter_to_channel;

	// I hate this function. It should only be called once per experiment run or for the entire progam
	// PinPoint should be adapted to call a source-class-wide "start now, for real" function on registered sources
	void update_subscriptions()
	{
		CFMutableDictionaryRef _sub_chans;
		active_subscription = cf_shared(IOReportCreateSubscription(NULL, desired_channels.get(), &_sub_chans, 0, 0));
		if (!active_subscription) {
			throw std::runtime_error("Cannot create IOReport subscription");
		}
		subscribed_channels = cf_shared(_sub_chans);
		initialize_experiment();
	}

	void read_and_update_all_open_sources()
	{
		auto current_samples = _pull_raw_samples();
		auto deltas = cf_shared(IOReportCreateSamplesDelta(initial_samples.get(), current_samples.get(), NULL));
		if (deltas) {
			IOReportIterate(deltas.get(), (ioreportiterateblock) ^(IOReportSampleRef sample) {
				auto key = buildCounterName(sample);
				last_values[key] = IOReportSimpleGetIntegerValue(sample, NULL);

				return kIOReportIterOk;
			});
		}
	}
	
	void add_available_channels(CFMutableDictionaryPtr channels)
	{
		IOReportIterate(channels.get(), (ioreportiterateblock) ^(IOReportChannelRef ch) {
			CFRetain(ch);
			counter_to_channel[buildCounterName(ch)] = ch;

			return kIOReportIterOk;
		});
	}
	
private:
	
	CFMutableDictionaryPtr _pull_raw_samples()
	{
		auto res = cf_shared(IOReportCreateSamples(active_subscription.get(), subscribed_channels.get(), NULL));
		if (!res) {
			throw std::runtime_error("Cannot pull IOReport, most likely API change :(");
		}
		return res;
	}
	
	void initialize_experiment()
	{
		initial_samples = _pull_raw_samples();
	}
};

static M1nPointSharedDetail m1npoint;


/***********************************************************************/

struct AppleMDetail
{
	// I don't want to do the myriad static members this time. Use m1npoint instead for now
	// Except for the kOutdated hack, so intertwined with last_value
	static const uint64_t kOutdated;

	std::string last_value_key;
};
const uint64_t AppleMDetail::kOutdated = std::numeric_limits<uint64_t>::max();



AppleM::AppleM(const std::string & key) :
	m_detail(new AppleMDetail)
{
	auto channel_wrap = cf_shared(channel2channel_list(m1npoint.counter_to_channel[key]));

	if (!m1npoint.desired_channels) {
		m1npoint.desired_channels = channel_wrap;
	} else {
		if (IOReportMergeChannels(m1npoint.desired_channels.get(), channel_wrap.get(), NULL) != kIOReportIterOk)
			throw std::runtime_error("IOReportMergeChannels failed. Most likely the undocumented API changed :(");
	}
	
	m_detail->last_value_key = key;
	m1npoint.last_values[m_detail->last_value_key] = AppleMDetail::kOutdated;
	m1npoint.update_subscriptions();
}

EnergySample AppleM::read_energy()
{
	if (m1npoint.last_values[m_detail->last_value_key]  == AppleMDetail::kOutdated)
		m1npoint.read_and_update_all_open_sources();
		
	units::energy::millijoule_t e(m1npoint.last_values[m_detail->last_value_key]);
	m1npoint.last_values[m_detail->last_value_key] = AppleMDetail::kOutdated;
	return EnergySample(e);
}

AppleM::~AppleM()
{
	delete m_detail;
}

/***********************************************************************/

std::vector<std::string> AppleM::detectAvailableCounters()
{
	m1npoint.add_available_channels(cf_shared(IOReportCopyChannelsInGroup(CFSTR("PMP"), CFSTR("Energy Counters"), 0, 0, 0)));
	m1npoint.add_available_channels(cf_shared(IOReportCopyChannelsInGroup(CFSTR("PMP"), CFSTR("DRAM Energy"), 0, 0, 0)));

	std::vector<std::string> result;
	for (const auto & counter_channel: m1npoint.counter_to_channel) {
		result.emplace_back(counter_channel.first);
	}
	return result;
}

PowerDataSourcePtr AppleM::openCounter(const std::string & counterName)
{
	if (m1npoint.counter_to_channel.find(counterName) == m1npoint.counter_to_channel.end())
		return nullptr;

	return PowerDataSourcePtr(new AppleM(counterName));
}

Aliases AppleM::possibleAliases()
{
	return Aliases();
}

PINPOINT_REGISTER_DATA_SOURCE(AppleM)

#endif
