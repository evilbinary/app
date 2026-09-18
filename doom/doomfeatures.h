//
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//     List of features which can be enabled/disabled to slim down the
//     program.
//

#ifndef DOOM_FEATURES_H
#define DOOM_FEATURES_H

// Enables wad merging (the '-merge' command line parameter)

#undef FEATURE_WAD_MERGE

// Enables dehacked support ('-deh')

#undef FEATURE_DEHACKED

// Enables multiplayer support (network games)

#undef FEATURE_MULTIPLAYER

// Enables sound output
//
// 【YiYiYa】打开音效：后端是 i_yiyiyasound.c（自己混音后写 /dev/dsp，见该文件注释），
// 不依赖 SDL_mixer。音乐（music_module_t）目前是空实现 —— Doom 的音乐是 MUS→MIDI，
// 还需要合成器，本平台先只做音效。

#define FEATURE_SOUND

#endif /* #ifndef DOOM_FEATURES_H */


