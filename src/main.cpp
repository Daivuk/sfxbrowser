#include <onut/Color.h>
#include <onut/ContentManager.h>
#include <onut/Files.h>
#include <onut/Json.h>
#include <onut/Renderer.h>
#include <onut/Settings.h>
#include <onut/Sound.h>
#include <onut/Strings.h>
#include <onut/Texture.h>

#include <imgui/imgui.h>

#include <json/json.h>

#include <memory>
#include <string>
#include <vector>


struct tag_t
{
    std::string name;
    Color color;
};


struct sfx_t
{
    bool selected = false;
    std::string name;
    std::string relative_path;
    std::string full_path;
    OSoundRef sound;
    std::vector<std::shared_ptr<tag_t>> tags;
};


struct folder_t
{
    std::string name;
    std::vector<std::shared_ptr<folder_t>> folders;
    std::vector<std::shared_ptr<sfx_t>> sfxs;
};


struct path_t
{
    bool selected = false;
    std::string url;
    std::vector<std::shared_ptr<sfx_t>> sfxs;
    std::shared_ptr<folder_t> folder;
};


std::vector<std::shared_ptr<path_t>> paths;
std::vector<tag_t> tags;
std::vector<std::shared_ptr<sfx_t>> cached_sounds;

OTextureRef speaker_texture;
OTextureRef folder_texture;
OSoundInstanceRef sound_instance;

size_t ram_usage = 0;
#define MAX_RAM_USAGE (1024 * 1024 * 500) // 500 MB enough?


static void populate_path(std::shared_ptr<path_t>& path)
{
    path->folder = std::make_shared<folder_t>();
    path->folder->name = onut::getFilename(path->url);
    auto files = onut::findAllFiles(path->url, "wav", true);
    for (const auto& file : files)
    {
        auto sfx = std::make_shared<sfx_t>();
        sfx->full_path = file;
        sfx->name = onut::getFilenameWithoutExtension(file);
        sfx->relative_path = onut::makeRelativePath(file, path->url);
        path->sfxs.push_back(sfx);

        auto parts = onut::splitString(sfx->relative_path, "/\\");
        auto parent = path->folder;
        for (int i = 0, len = (int)parts.size() - 1; i < len; ++i)
        {
            auto& part = parts[i];
            bool found = false;
            for (auto& folder : parent->folders)
            {
                if (folder->name == part)
                {
                    parent = folder;
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                auto folder = std::make_shared<folder_t>();
                folder->name = part;
                parent->folders.push_back(folder);
                parent = folder;
            }
        }
        parent->sfxs.push_back(sfx);
    }
}

void initSettings()
{
    oSettings->setGameName("SFX Browser");
    oSettings->setResolution({ 1600, 900 });
    oSettings->setShowFPS(false);
    oSettings->setIsFixedStep(false);
    oSettings->setAntiAliasing(false);
    oSettings->setIsResizableWindow(true);
    oSettings->setShowOnScreenLog(true);
    oSettings->setStartMaximized(true);
}

void init()
{
    speaker_texture = OGetTexture("speaker.png");
    folder_texture = OGetTexture("folder.png");

    Json::Value json_config;
    if (onut::loadJson(json_config, onut::getSavePath() + "configs.json"))
    {
        Json::Value json_paths = json_config["paths"];
        for (const auto& json_path : json_paths)
        {
            auto path = std::make_shared<path_t>();
            path->url = json_path.asString();
            paths.push_back(path);
            populate_path(path);
        }
    }
}

void shutdown()
{
    Json::Value json_config;
    Json::Value json_paths(Json::arrayValue);
    for (const auto& path : paths)
    {
        json_paths.append(path->url);
    }
    json_config["paths"] = json_paths;
    onut::saveJson(json_config, onut::getSavePath() + "configs.json", true);
}

void update()
{
}

void render()
{
    oRenderer->clear(Color::Black);
}

static void play_sfx(std::shared_ptr<sfx_t>& sfx)
{
    if (sound_instance) sound_instance->stop(); sound_instance = nullptr;

    bool was_cached = false;
    for (int i = 0, len = (int)cached_sounds.size(); i < len; ++i)
    {
        const auto& cached_sound = cached_sounds[i];
        if (cached_sound == sfx)
        {
            was_cached = true;
            cached_sounds.erase(cached_sounds.begin() + i);
            cached_sounds.insert(cached_sounds.begin(), sfx);
            break;
        }
    }

    if (!was_cached)
    {
        sfx->sound = OSound::createFromFile(sfx->full_path);
        ram_usage += sfx->sound->getSize();

        // Clear other sounds if over the limit
        while (ram_usage > MAX_RAM_USAGE && !cached_sounds.empty())
        {
            auto back = cached_sounds.back();
            ram_usage -= back->sound->getSize();
            back->sound.reset();
            cached_sounds.pop_back();
        }

        cached_sounds.insert(cached_sounds.begin(), sfx);
    }

    // Cap maximum of loaded sounds? Should do something smart with ram usage or something

    if (sfx->sound)
    {
        sound_instance = sfx->sound->createInstance();
        sound_instance->play();
    }

}

static void browser_folder(const std::shared_ptr<folder_t>& folder)
{
    ImGui::Image(&folder_texture, ImVec2(16, 16), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0.75f, 0.75f, 0.75f, 1)); ImGui::SameLine();
    if (ImGui::TreeNode(folder->name.c_str()))
    {
        for (const auto& subfolder : folder->folders)
        {
            browser_folder(subfolder);
        }
        for (auto& sfx : folder->sfxs)
        {
            ImGui::Image(&speaker_texture, ImVec2(16, 16)); ImGui::SameLine();
            if (ImGui::Selectable(sfx->name.c_str()))
            {
                play_sfx(sfx);
            }
        }
        ImGui::TreePop();
    }
}

void renderUI()
{
    if (ImGui::BeginMainMenuBar())
    {
        ImGui::Text("Ram usage: %i MB / %i MB", ram_usage / 1024 / 1024, MAX_RAM_USAGE / 1024 / 1024);
        ImGui::EndMainMenuBar();
    }

    if (ImGui::Begin("Paths"))
    {
        if (ImGui::Button("+"))
        {
            auto path_str = onut::showOpenFolderDialog("Add Path");
            if (!path_str.empty())
            {
                auto path = std::make_shared<path_t>();
                path->url = path_str;
                for (auto& other_path : paths) other_path->selected = false;
                path->selected = true;
                populate_path(path);
                paths.push_back(path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("-"))
        {
        }

        for (const auto& path : paths)
        {
            ImGui::Selectable(path->url.c_str(), &path->selected);
        }
    }
    ImGui::End();

    if (ImGui::Begin("Browser"))
    {
        for (const auto& path : paths)
        {
            browser_folder(path->folder);
        }
    }
    ImGui::End();
}

void postRender()
{
}
