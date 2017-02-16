import os
import PIL.Image as image
import sys

path = sys.argv[0]
files = os.listdir(path)

with open(os.path.join(path, "Output.txt"), "w") as text_file:
    for file in files:
        I = image.open(os.path.join(path, file))
        pix = I.load()
        width, height = I.size

        for i in range(width):
            for j in range(height):
                value = pix[i, j][0]
                if value < 50: value = 0
                print(value, file=text_file, end=";")