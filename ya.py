# coding:utf-8
# *******************************************************************
# * Copyright 2021-present evilbinary
# * 作者: evilbinary on 01/01/20
# * 邮箱: rootdebug@163.com
# ********************************************************************
if has_config("app"):
    for v in get_config('apps'):
        includes("./"+v+"/ya.py")

if has_config("cpp-apps"): 
    for v in get_config('cpp-apps'):
        includes("./"+v+"/ya.py")