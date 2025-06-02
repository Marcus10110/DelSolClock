Import("env")


def override_bootloader_path(source, target, env):
    custom_bootloader = "esp32_images/bootloader.bin"

    flags = env["UPLOADERFLAGS"]
    for i, flag in enumerate(flags):
        if str(flag).endswith("bootloader.bin") and i > 0:
            print(f"⚙️ Replacing bootloader: {flags[i]} → {custom_bootloader}")
            flags[i] = custom_bootloader
            break


env.AddPreAction("upload", override_bootloader_path)
