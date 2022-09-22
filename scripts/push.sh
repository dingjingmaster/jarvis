#!/bin/bash

pkg_name='jarvis'
work_dir=$(dirname $(dirname $(realpath -- $0)))
build=$(which makepkg)

echo $pkg_name
echo $work_dir
cd $work_dir

[ -d $work_dir/$pkg_name ] && rm -rf $work_dir/$pkg_name

cd $work_dir && git clone ssh://aur@aur.archlinux.org/$pkg_name.git

[ $? -ne 0 ] && echo 'execute error!' && exit
[ ! -d $work_dir/$pkg_name ] && echo "$pkg_name project not exists!" && exit 1

cd $work_dir && cp LICENSE pkg/archlinux/PKGBUILD README.md $work_dir/$pkg_name

[ ! -e $build ] && echo 'makepkg command not found!' && exit 1
cd $work_dir/$pkg_name && $build --printsrcinfo > .SRCINFO

# build
cd $work_dir/$pkg_name && $build
