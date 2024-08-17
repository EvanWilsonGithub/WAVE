import pathlib
import os
import sys
os.walk
dir = pathlib.Path(sys.argv[1]).iterdir()
os.system("rd /s /q out")
os.system("rd /s /q wav")
os.mkdir("out")
os.mkdir("wav")
i = 0
for item in dir:
    if item.is_dir():
        os.mkdir("out/" + item.name)
        os.mkdir("wav/" + item.name)
        subdir = pathlib.Path(item).iterdir()
        for item2 in subdir:
            os.system(f"{sys.argv[2]}/ww2ogg.exe --pcb {sys.argv[2]}/packed_codebooks.bin {item2} -o out/{item.name}/{item2.with_suffix(".ogg").name}")
            os.system(f"{sys.argv[2]}/ffmpeg -y -i out/{item.name}/{item2.with_suffix(".ogg").name} wav/{item.name}/{item2.with_suffix(".wav").name}")


    else:
        os.system(f"{sys.argv[2]}/ww2ogg.exe --pcb {sys.argv[2]}/packed_codebooks.bin {item}  -o out/{item.with_suffix(".ogg").name}")
        os.system(f"{sys.argv[2]}/ffmpeg -y -i out/{item.with_suffix(".ogg").name} wav/{item.with_suffix(".wav").name} ")

os.system("rd /s /q out")
os.system(f"rd /s /q {sys.argv[1]}")
os.system(f"tar.exe -a -c -f {sys.argv[1].split("/")[-1]}.zip wav")
os.system("rd /s /q wav")

notification = f"powershell -executionpolicy bypass \"{sys.argv[2]}\\toast.ps1\""
os.system(notification)