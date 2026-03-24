# Troubleshooting

## cmake build fail (Conda env conflcit)
- Problem: `GLIBCXX_3.4.30 not found`
- Cause: Conda's lower version of `libstdc++`
- Resolve: Deactivate conda
```bash
which cmake
/usr/bin/cmake --version
```

## /boot partition shortage
- Problem: mkinitramfs failure zstd (during make install)
- Cause: Previous version of `initrd` file
- Resolve: 
```bash
sudo rm /boot/initrd.img-6.8.0.old
sudo rm /boot/System.map-6.8.0.old
sudo rm /boot/config-6.8.0.old
```

## Access after `kfree()` during `hash_for_each_possible`
- Problem: Compile error
- Cause: During hash table traversal, dangling pointer
- Resolve: `break` the loop after `hash_del()` + `kfree()`

## Kernel panic
- Problem: Kernel panic - not syncing: System is deadlocked on memory
- Cause: lack of QEMU Memory
- Resolve: qemu-system-x86_64 -m 8G

