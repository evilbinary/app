target("launcher")
set_type("cli")

add_deps("lvqrcode")

add_files(
    'main.c',
    'launcher.c',
    'assets/weather.c',
    'assets/wallet.c',
    'assets/voice.c',
    'assets/under_construction.c',
    'assets/translate.c',
    'assets/stocks.c',
    'assets/shortcuts.c',
    'assets/settings.c',
    'assets/safari.c',
    'assets/reminders.c',
    'assets/podcasts.c',
    'assets/pi_menu.c',
    'assets/photos.c',
    'assets/notes.c',
    'assets/news.c',
    'assets/nes_menu.c',
    'assets/music.c',
    'assets/mouse_gray.c',
    'assets/mouse_black.c',
    'assets/mouse.c',
    'assets/message.c',
    'assets/measure.c',
    'assets/mail.c',
    'assets/linux_menu.c',
    'assets/launcher_menu.c',
    'assets/duck_menu.c',

    'assets/home.c',
    'assets/health.c',
    'assets/find.c',
    'assets/files.c',
    'assets/facetime.c',
    'assets/contacts.c',
    'assets/clocks.c',
    'assets/clips.c',
    'assets/camera.c',
    'assets/calendar.c',
    'assets/calculator.c',
    'assets/calcuator.c',
    'assets/books.c',
    'assets/backhome.c',
    # 'assets/background_dock.c',
    'assets/background_dock_480x272.c',
    'assets/background.c',
    'assets/appstore.c',
    'assets/appletv.c',
    'assets/applestore.c'
) 


add_cflags('-DLV_LVGL_H_INCLUDE_SIMPLE=1')
