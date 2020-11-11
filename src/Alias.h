#pragma once

#include <map>
#include <string>

/* For now aliases maps aliasname -> [sourcename:]countername,
 * where sourcename is added by the Registry.
 * This might change as we introduce priorites, etc.
 */

using Aliases = std::map<std::string, std::string>;
