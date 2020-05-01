#!/usr/bin/env python3
import os
import sys
import binascii
import subprocess

os.chdir(os.path.dirname(__file__) + "/..")

SHADERS = [
	("datasrc/shaders/main.vert", "Vert"),
	("datasrc/shaders/no-tex.frag", "FragNoTex"),
	("datasrc/shaders/2d.frag", "Frag2D"),
    ("datasrc/shaders/2d-array.frag", "Frag2DArray")
]

OUTPUT = "src/engine/client/shaders.h"

def bin2array(data):
    out = []
    hex = binascii.hexlify(data).decode("UTF-8")
    lines = [hex[i:i+32] for i in range(0, len(hex), 32)]
    for line in lines:
        values = ["0x" + line[i:i + 2] for i in range(0, len(line), 2)]
        out.append("\t{},".format(", ".join(values)))
    return "\n".join(out)

def compile(file):
    try:
        spirv = subprocess.check_output(["glslc", "-O", "-o", "-", file])
    except subprocess.CalledProcessError as e:
        print(e)
        return None
    return spirv

def compile_shaders(shaders, output):
    shaders_spirv = []
    for shader in shaders:
        spirv = compile(shader[0])
        if(not spirv):
            sys.exit(-1)
        shaders_spirv.append((shader[1], spirv))
    
    with open(output, "w") as file:
        for shader in shaders_spirv:
            file.write("static const unsigned char s_a{}[] = {{\n".format(shader[0]))
            file.write(bin2array(shader[1]))
            file.write("\n};\n\n")

if __name__ == '__main__':
	compile_shaders(SHADERS, OUTPUT)