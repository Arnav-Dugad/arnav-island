#pragma once
#include <string>
#include <vector>
namespace nexus {
// Titles of every open tab in running browsers, read locally through UI
// Automation from each window's tab strip. Web page content is never walked:
// document subtrees are skipped, so browsers are not asked to build page
// accessibility trees. A window without a tab strip (an installed web app, for
// example) contributes its window title. Must run on a COM (MTA) thread.
std::vector<std::wstring> browserTabTitles();
}
