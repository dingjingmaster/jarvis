#!/bin/bash

#set -e

pkgver=0.0.3
pkg_name='jarvis'
work_dir=$(dirname $(dirname $(realpath -- $0)))
pkg_dir=${work_dir}/pkg
build=$(which makepkg)
source_tar="https://github.com/dingjingmaster/jarvis/archive/refs/tags/${pkgver}.tar.gz"

echo "    包名: $pkg_name"
echo "工作路径: $work_dir"
echo "打包路径: $pkg_dir"

cd $work_dir

echo '开始推送新版本...'
git tag -f ${pkgver} > /dev/null 2>&1 && git push --tag > /dev/null 2>&1
[ $? -ne 0 ] && echo '版本推送出错!' && exit
echo '推送版本OK!'


echo '开始拉取AUR仓库...'
[ -d $pkg_dir/$pkg_name/.git ] && rm -rf ${pkg_dir}/${pkg_name}
git clone ssh://aur@aur.archlinux.org/$pkg_name.git ${pkg_dir}/${pkg_name} > /dev/null 2>&1
[ $? -ne 0 ] && echo '拉取AUR仓库失败!' && exit 
echo '拉取AUR仓库OK!'


echo '开始拉取 tag 文件...'
[ -d ${pkg_dir}/${pkgver}.tar.gz ] && rm -rf ${pkg_dir}/${pkgver}.tar.gz
wget -t 3 ${source_tar} -O ${pkg_dir}/${pkgver}.tar.gz > /dev/null 2>&1
[ $? -ne 0 ] && echo '拉取tag文件出错!' && exit
echo '拉取 tag 文件OK!'

echo '开始生成 PKGCONFIG 文件...'
cp LICENSE README.md $pkg_dir/$pkg_name/
pkg_md5=$(sha512sum -b "${pkg_dir}/${pkgver}.tar.gz" | awk -F' ' '{print $1}')

cat>${pkg_dir}/$pkg_name/PKGBUILD<<EOF
# Maintainer: dingjing <dingjing@live.cn>

pkgname=${pkg_name}
pkgver=${pkgver}
pkgrel=1
pkgdesc='一款数据分析与展示平台(版本号高于 1.0.0 才可正常使用)'
url='https://github.com/dingjingmaster/jarvis'
arch=('x86_64')
license=('MIT')
depends=('openssl' 'sqlite3')
makedepends=('cmake')
source=("${source_tar}")
sha512sums=('${pkg_md5}')

prepare() {
    cd \${srcdir}/\${pkgname}-\$pkgver 
    [ ! -d build ] && mkdir build
    cd build
    cmake -D DEBUG=OFF ..
}

build() {
    cd \${srcdir}/\${pkgname}-\${pkgver}/build
    make jarvis
}

package() {
    msg "make \${pkgname} package ..."
    cd \${srcdir}/\${pkgname}-\${pkgver}
    msg \${srcdir}/\${pkgname}-\${pkgver}

    install -dm755 "\${pkgdir}/var/lib/jarvis/db/"
    install -dm755 "\${pkgdir}/usr/lib/systemd/system/"
    install -dm755 "\${pkgdir}/usr/local/\${pkgname}/bin"
    install -dm755 "\${pkgdir}/usr/share/doc/\${pkgname}/"
    install -dm755 "\${pkgdir}/usr/share/licenses/\${pkgname}/"

    install -Dm755 \${srcdir}/\${pkgname}-\${pkgver}/build/app/\${pkgname}              "\${pkgdir}/usr/local/\${pkgname}/bin/"
    install -Dm755 \${srcdir}/\${pkgname}-\${pkgver}/data/\${pkgname}.service           "\${pkgdir}/usr/lib/systemd/system/\${pkgname}.service"

    install -Dm755 ../../README.md                                                      "\${pkgdir}/usr/share/doc/\${pkgname}/README"
    install -Dm755 ../../LICENSE                                                        "\${pkgdir}/usr/share/licenses/\${pkgname}/LICENSE"
}
EOF

cat>${pkg_dir}/${pkg_name}/.gitignore<<EOF
*.tar.gz
*.tar.zst
pkg/
src/
EOF

cat>${pkg_dir}/${pkg_name}/${pkg_name}.install<<EOF
pre_install() {
}

post_install() {
    systemctl enable jarvis.service
    systemctl start jarvis.service
}

pre_upgrade() {
}

post_upgrade() {
}

pre_remove() {
    systemctl stop jarvis
}
post_remove() {
    systemctl disable jarvis
}
EOF

[ ! -e $build ] && echo '命令: makepkg 未找到!' && exit 1
cd $pkg_dir/$pkg_name && $build --printsrcinfo > .SRCINFO

# build
cd $pkg_dir/$pkg_name && $build -f

# push
cd $pkg_dir/$pkg_name
[ $? -ne 0 ] && echo '构建出错!' && exit
git add -A && git commit -m "${pkg_name} V${pkgver}" && git push -f

[ $? -eq 0 ] && echo 'OK!' && exit 0

