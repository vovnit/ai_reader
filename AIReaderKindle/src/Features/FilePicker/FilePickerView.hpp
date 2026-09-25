#pragma once

#include <functional>
#include <string>

class Navigator;

/// A screen for choosing a file by walking the folders, one tall row per
/// entry. Picking a file closes the screen and hands its path to `onPick`.
namespace FilePickerView {

void open(
    Navigator& navigator,
    const std::string& title,
    std::function<bool(const std::string& path)> accepts,
    std::function<void(const std::string& path)> onPick);

}  // namespace FilePickerView
