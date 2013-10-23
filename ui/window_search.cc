/* vim:set ts=4 sw=4 sts=4 et cindent: */
/*
 * nanodc - The ncurses DC++ client
 * Copyright © 2005-2006 Markus Lindqvist <nanodc.developer@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Contributor(s):
 *  
 */

#include <algorithm>
#include <sstream>
#include <iomanip>
#include <functional>
#include <ui/window_search.h>
#include <utils/utils.h>
#include <utils/strings.h>
#include <client/DirectoryListingManager.h>
#include <input/completion.h>

namespace ui {

WindowSearch::WindowSearch(const std::string &str):
    m_shutdown(false),
    m_property(PROP_NONE),
    m_lastSearch(0),
    m_search(str),
    m_minSize(0),
    m_maxSize(0),
    m_freeSlots(false),
	ListView(display::TYPE_SEARCHWINDOW, str)
{
    SearchManager::getInstance()->addListener(this);

    set_title("Search");

    // hidden column for identifying the rows
    insert_column(new display::Column("User-TTH"));
	insert_column(new display::Column("User", 10, 15, 30));
    insert_column(new display::Column("Slots", 6, 7, 8));
    insert_column(new display::Column("Size", 11, 11, 15));
	insert_column(new display::Column("Date", 11, 11, 11));
    insert_column(new display::Column("File name", 50, 200, 200));
    resize();

    if(!m_search.empty())
        search(m_search);

    // download
    m_bindings['d'] = std::bind(&WindowSearch::download, this, SETTING(DOWNLOAD_DIRECTORY));

    // download to..
    m_bindings['D'] = std::bind(&WindowSearch::set_property, this, PROP_FILETARGET);

    // download directory
    m_bindings['s'] = std::bind(&WindowSearch::download, this, SETTING(DOWNLOAD_DIRECTORY));

    // download directory to..
    m_bindings['S'] = std::bind(&WindowSearch::set_property, this, PROP_DIRECTORYTARGET);

    // browse
	m_bindings['b'] = std::bind(&WindowSearch::handleGetList, this);
    // match queue
	m_bindings['M'] = std::bind(&WindowSearch::handleMatchQueue, this);
    // search
    m_bindings['r'] = std::bind(&WindowSearch::search, this, std::string());

    m_bindings['l'] = std::bind(&WindowSearch::toggle_slots, this);
    m_bindings['n'] = std::bind(&WindowSearch::set_property, this, PROP_MINSIZE);
    m_bindings['m'] = std::bind(&WindowSearch::set_property, this, PROP_MAXSIZE);
    m_bindings['e'] = std::bind(&WindowSearch::set_property, this, PROP_EXTENSION);
    m_bindings['f'] = std::bind(&WindowSearch::set_property, this, PROP_SEARCHFILTER);
    m_bindings['/'] = m_bindings['f'];
    m_bindings['c'] = std::bind(&WindowSearch::free_results, this);
}

void WindowSearch::handleGetList() {
	if (get_selected_row() == -1)
		return;

	try {
		QueueManager::getInstance()->addList(get_user(), QueueItem::FLAG_CLIENT_VIEW, get_result()->getPath());
	} catch (...) {

	}
}

void WindowSearch::handleMatchQueue() {
	if (get_selected_row() == -1)
		return;

	try {
		QueueManager::getInstance()->addList(get_user(), QueueItem::FLAG_MATCH_QUEUE);
	} catch (...) {

	}
}

SearchResultPtr WindowSearch::get_result() {
	int row = get_selected_row();
	if (row == -1) {
		return nullptr;
	}

    auto cid = get_text(0, get_selected_row());
    auto n = cid.find('-');
    auto tth = cid.substr(n+1);
    cid = cid.substr(0, n);

    for(unsigned int i=0; i<m_results.size(); ++i) {
        auto result = m_results[i];
        if(cid == result->getUser().user->getCID().toBase32() &&
           tth == result->getTTH().toBase32())
        {
            return result;
        }
    }
    return nullptr;
}

void WindowSearch::set_property(Property property)
{
    m_property = property;
	setInsertMode(true);
    const char *text[] = {
        "",
        "Search string?",
        "Minimum size?",
        "Maximum size?",
        "File extension?",
        "Download file to:",
        "Download directory to:"
    };
    m_prompt = text[m_property];
}

void WindowSearch::search(const std::string &str)
{
    /*
    if(m_lastSearch && m_lastSearch + 3*1000 < TimerManager::getInstance()->getTick()) {
        core::Log::get()->log("Wait a moment before a new search");
        return;
    }
    */

    if(str.length() < MIN_SEARCH  && m_search.length() < MIN_SEARCH) {
        core::Log::get()->log("Too short search");
        return;
    }

    // new search
    if(!str.empty()) {
        m_searchWords.clear();
        m_search = Text::toLower(str);
		m_searchWords = AdcSearch::parseSearchString(m_search);

        utils::Lock lock(m_resultLock);
        m_results.clear();
    }

    set_title("Search window: " + m_search);
    set_name("Search:" + m_search);

    m_lastSearch = GET_TICK();
	token = Util::toString(Util::rand());
	SearchManager::getInstance()->search(m_search, 0, SearchManager::TYPE_ANY, SearchManager::SIZE_DONTCARE, token, Search::MANUAL);
}

void WindowSearch::handle_line(const std::string &line)
{
	if (!getInsertMode())
		return;

    if(!line.empty()) {
        if(m_property == PROP_FILETARGET) {
            download(line);
        }
        else if(m_property == PROP_DIRECTORYTARGET) {
            download_directory(line);
        }
        else if(m_property == PROP_SEARCHFILTER) {
            if(line.length() >= MIN_SEARCH) {
                m_search = utils::tolower(line);
                m_searchWords.clear();
                strings::split(m_search, " ", std::back_inserter(m_searchWords));
            }
            else {
                m_property = PROP_NONE;
                return;
            }
        }
        else if(m_property == PROP_MINSIZE || m_property == PROP_MAXSIZE) {
            std::istringstream oss(line);
            int64_t size;
            oss >> size;
            int c = oss.get();

            switch(c) {
                case 'k':
                    size *= 1024;
                    break;
                case 'G':
                    size *= 1024*1024*1024;
                    break;
                case 'M':
                default:
                    size *= 1024*1024;
                    break;
            }
            if(m_property == PROP_MINSIZE)
                m_minSize = size;
            else
                m_maxSize = size;
        }
        else if(m_property == PROP_EXTENSION) {
            m_extensions.clear();
            strings::split(utils::tolower(line), ",", std::back_inserter(m_extensions));
        }
    }

	setInsertMode(false);
    set_prompt("");

    if(!line.empty() && m_property != PROP_FILETARGET &&
        m_property != PROP_DIRECTORYTARGET)
    {
        create_list();
    }
    m_property = PROP_NONE;
}

void WindowSearch::complete(const std::vector<std::string>& aArgs, int /*pos*/, std::vector<std::string>& suggest_, bool& appendSpace_) {
	if (m_property == PROP_FILETARGET || m_property == PROP_DIRECTORYTARGET) {
		input::Completion::getDiskPathSuggestions(aArgs[0], suggest_);
		appendSpace_ = false;
	}
}

void WindowSearch::handleEscape() {
	m_property = PROP_NONE;
}

void WindowSearch::create_list()
{
    delete_all();

    m_resultLock.lock();
    //std::for_each(m_results.begin(), m_results.end(),
    //    std::bind(&WindowSearch::add_result, this,
   //         std::placeholders::_1));
    m_resultLock.unlock();

    std::ostringstream oss;
    oss << "Search: " << m_search << " with " << get_size()
        << "/" << m_results.size() << " results";
    set_title(oss.str());

    set_name("Search:" + m_search);
}

void WindowSearch::on(SearchManagerListener::SR, const SearchResultPtr& result)
    noexcept
{
    try {
        m_resultLock.lock();
        m_results.push_back(result);
        m_resultLock.unlock();

        add_result(result);

        std::ostringstream oss;
        oss << "Search: " << m_search << " with " << get_size()
            << "/" << m_results.size() << " results";
        set_title(oss.str());
    } catch(const Exception &e) {
        core::Log::get()->log("WindowSearch::on(): Exception " + e.getError());
    }
    catch(std::exception &e) {
        try {
            core::Log::get()->log(std::string("WindowSearch::on(): std::exception ") + e.what());
        } catch(std::exception &e) {
            core::Log::get()->log("[wtf]?");
        }
    }
}

void WindowSearch::add_result(const SearchResultPtr& result)
{
    if(matches(result)) {
        int row = insert_row();
        set_text(0, row, result->getUser().user->getCID().toBase32() + "-" + result->getTTH().toBase32());
		set_text(1, row, ClientManager::getInstance()->getFormatedNicks(result->getUser()));
        set_text(2, row, utils::to_string(result->getFreeSlots())
            + "/" + utils::to_string(result->getSlots()));
        set_text(3, row, Util::formatBytes(result->getSize()));
		set_text(4, row, Util::getDateTime(result->getDate()));
		set_text(5, row, strings::escape(result->getFileName()));
    }
}

bool WindowSearch::matches(const SearchResultPtr& result)
{
	if (!result->getUser().user->isNMDC()) {
		return result->getToken() == token;
	}

    auto filename = utils::tolower(result->getFileName());

    if(!utils::find_in_string(filename, m_searchWords.begin(), m_searchWords.end())) {
        return false;
    }
    if(result->getSize() < m_minSize) {
        return false;
    }
    if(m_maxSize && result->getSize() > m_maxSize) {
        return false;
    }

    if(m_freeSlots && result->getFreeSlots() < 1) {
        return false;
    }

    int matches = 0;
    for(unsigned int i=0; i<m_extensions.size(); ++i) {
        auto extension = m_extensions.at(i);
        if(filename.find_last_of(extension) == filename.length()-1) {
            matches++;
        }
    }

    if(matches || m_extensions.empty())
        return true;

    return false;
}

void WindowSearch::free_results()
{
    delete_all();
    utils::Lock lock(m_resultLock);
    m_results.clear();
}

string getTarget(const string& aTarget) {
	auto target = aTarget.empty() ? SETTING(DOWNLOAD_DIRECTORY) : aTarget;
	if (!target.empty() && target[target.length() - 1] != '/')
		target += "/";
	return target;
}

void WindowSearch::download(const std::string &path)
{
    auto result = get_result();
	if (!result)
		return;

	auto target = getTarget(path);

    try {
        if(result->getType() == SearchResult::TYPE_FILE) {
            std::ostringstream oss;
            oss << "download to " << target << " "
                << result->getTTH().toBase32()
                << ", " << result->getPath();
            core::Log::get()->log(oss.str());

			std::string subdir = Util::getFileName(utils::linux_separator(result->getPath()));

            QueueManager::getInstance()->createFileBundle(
                target + subdir,
                result->getSize(), result->getTTH(),
                result->getUser(), result->getDate(), 0, QueueItemBase::DEFAULT);
        } else {
			DirectoryListingManager::getInstance()->addDirectoryDownload(result->getPath(), result->getFileName(),
				result->getUser(), target, TargetUtil::TARGET_PATH, NO_CHECK);
        }
    }
    catch(const Exception &e)
    {
        core::Log::get()->log("Error downloading the file: " + e.getError());
    }
}

void WindowSearch::download_directory(const std::string &path)
{
    auto result = get_result();
	if (!result)
		return;

	auto target = getTarget(path);
    try {
        if(result->getType() == SearchResult::TYPE_FILE)
        {
			DirectoryListingManager::getInstance()->addDirectoryDownload(utils::windows_separator(Util::getNmdcFilePath(utils::linux_separator(result->getPath()))),
                Util::getNmdcLastDir(result->getFilePath()), result->getUser(),
                target.empty() ? SETTING (DOWNLOAD_DIRECTORY) : target + "/", TargetUtil::TARGET_PATH, NO_CHECK);
        }
        else
        {
			DirectoryListingManager::getInstance()->addDirectoryDownload(result->getPath(), result->getFileName(),
				result->getUser(), target, TargetUtil::TARGET_PATH, NO_CHECK);
        }
    }
    catch(Exception &e)
    {
        core::Log::get()->log("Error downloading the directory: " + e.getError());
    }
}

std::string WindowSearch::get_infobox_line(unsigned int n)
{
    auto result = get_result();
	if (!result)
		return "";

    std::stringstream ss;
    switch(n) {
        case 1:
            ss << "%21User:%21 " << std::left << std::setw(18) 
				<< ClientManager::getInstance()->getFormatedNicks(result->getUser())
                << " %21IP:%21 " << result->getIP();
            break;
        case 2:
            ss << "%21Size:%21 " << std::left << std::setw(9) 
                << Util::formatBytes(result->getSize())
                << " %21Slots:%21 " << result->getFreeSlots() << "/" << result->getSlots();
            break;
        case 3:
			ss << "%21Hub:%21 " << std::left << std::setw(18) 
				<< ClientManager::getInstance()->getFormatedHubNames(result->getUser())
				<< " %21Date:%21 " << Util::getDateTime(result->getDate());
            break;
        case 4:
        {
			ss << strings::escape(Util::toAdcFile(result->getPath()));
            break;
        }
    }
    return ss.str();
}

WindowSearch::~WindowSearch()
{
    SearchManager::getInstance()->removeListener(this);
    utils::Lock lock(m_resultLock);
}

} // namespace ui
