# qua-linux

Qua Station向けのLinuxカーネルソースコードです。[tsukumijimaさんが改造を加えたコード](https://github.com/tsukumijima/QuaStation-Ubuntu)を参考にしながら、様々な機能をLinux mainlineに移植したものです。詳細な変更は[CHANGES.md](CHANGES.md)を参照してください。

## 免責

この手順ではQua Stationに対しカーネルやDTBを書き込む操作を行います。操作を誤るとQua Stationが起動しなくなる可能性があるため、自己責任で行ってください。私はこの手順による損害について一切責任を負いません。

このリポジトリの変更差分は全てClaude Codeによって作成されています。

## ビルド方法

Linux環境で以下の手順を実行してください。WSLでもビルド可能です。

ビルドにはaarch64向けのクロスコンパイラと、カーネルのビルドに必要なツールが要ります。Debian/Ubuntu(WSL含む)の場合:

```sh
sudo apt install git make gcc gcc-aarch64-linux-gnu bc bison flex libssl-dev kmod
```

```sh
git clone --depth 1 https://github.com/u-haru/qua-linux.git
cd qua-linux
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig quastation.config
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc) Image modules realtek/rtd1295-quastation.dtb
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- INSTALL_MOD_PATH=modinst INSTALL_MOD_STRIP=1 modules_install
tar -C modinst/lib/modules --exclude='*/build' --owner=0 --group=0 -czf modules.tar.gz .
```

## インストール

Qua Station上でLinuxが動いている前提です。まだ動かしていない場合は、私の[ブログ](https://www.u-haru.com/post/quastation用のarchlinuxイメージを作成する/)等を参考にUSBからLinuxを起動してください。モジュールやらbootargsやらは都度読み替えてください。

USBメモリなどで以下のファイルをQua Station側から読めるようにしてください。`/mnt/usb`にマウントされていることを想定しています。

- `arch/arm64/boot/Image`
- `arch/arm64/boot/dts/realtek/rtd1295-quastation.dtb`
- `modules.tar.gz`

その後、Qua Station上で以下のコマンドを実行してカーネルとDTBを書き込んでください。

```sh
sudo dd if=/mnt/usb/Image of=/dev/mmcblk0 bs=512 seek=$((0x1512A)) conv=fsync,notrunc
sudo dd if=/mnt/usb/rtd1295-quastation.dtb of=/dev/mmcblk0 bs=512 seek=$((0x3181)) conv=fsync,notrunc
sudo tar -C /usr/lib/modules -xzf /mnt/usb/modules.tar.gz
```

## U-Bootの設定

Qua Stationを起動し、シリアルコンソールでEscを連打してU-Bootのプロンプトを表示させます。
その後以下のコマンドを実行して、U-Bootが新しいカーネルとDTBを使用するように設定してください。
`root=/dev/mmcblk0p1`はルートファイルシステムの場所に応じて変更してください。

```
env set bootcmd "mmc read 0x03000000 1512a 10000; mmc read 0x01f00000 3181 80; env set bootargs earlycon=uart8250,mmio32,0x98007800 console=ttyS0,115200n8 root=/dev/mmcblk0p1 rw rootwait panic=10; env set bootcmd 'booti 0x03000000 - 0x01f00000'; b2ndbc"
env save
```

## 参考リンク

- [tsukumijima/QuaStation-Ubuntu](https://github.com/tsukumijima/QuaStation-Ubuntu)
- [Qua Station改造ブログ](https://www.u-haru.com/tags/quastation/)