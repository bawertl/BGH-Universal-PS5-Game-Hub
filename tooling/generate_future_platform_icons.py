"""BGH monochrome platform marks; generated from basic shapes, not brand image files.

96x96 transparent pixels, visually compatible with the native console silhouettes.
"""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
import math
OUT = Path(__file__).resolve().parents[1] / 'assets' / 'platforms'
OUT.mkdir(parents=True,exist_ok=True)
S=4
FONT='/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf'
COL='#EDF4FF'
SHADE='#91AFC5'

def canvas():
    im=Image.new('RGBA',(96*S,96*S),(0,0,0,0))
    return im,ImageDraw.Draw(im)
def ell(d,rect,fill=COL,width=0):
    box=tuple(round(t*S) for t in rect)
    d.ellipse(box,fill=fill if not width else None,outline=fill if width else None,width=width*S)
def line(d,pts,fill=COL,width=3):
    d.line([(round(x*S),round(y*S)) for x,y in pts],fill=fill,width=width*S,joint='curve')
def text(d,word,y,size=29):
    font=ImageFont.truetype(FONT,size*S)
    bbox=d.textbbox((0,0),word,font=font,stroke_width=0)
    w=bbox[2]-bbox[0]; h=bbox[3]-bbox[1]
    d.text(((96*S-w)/2-bbox[0],y*S-bbox[1]),word,font=font,fill=COL)
def end(im,name):
    im.resize((96,96),Image.Resampling.LANCZOS).save(OUT/(name+'.png'))

im,d=canvas(); text(d,'PS',24,31);text(d,'1',58,25);end(im,'ps1')
im,d=canvas();text(d,'PSP',36,25);line(d,[(18,69),(78,69)],SHADE,2);end(im,'psp')
im,d=canvas();text(d,'PS3',38,28);end(im,'ps3')
im,d=canvas();ell(d,(12,12,84,84),width=5);line(d,[(31,28),(65,67)],COL,7);line(d,[(65,28),(31,67)],COL,7);end(im,'xbox')
im,d=canvas();
# Dreamcast-inspired spiral in an unambiguous monochrome silhouette.
pts=[]
for i in range(145):
 t=i/144*math.pi*3.3;r=31*(1-i/170)
 pts.append((48+r*math.cos(t),48+r*math.sin(t)))
line(d,pts,COL,5);end(im,'dreamcast')
im,d=canvas();line(d,[(47,11),(81,30),(81,67),(47,85),(13,67),(13,30),(47,11)],COL,5)
line(d,[(13,30),(47,50),(81,30)],COL,4);line(d,[(47,50),(47,85)],SHADE,4)
end(im,'gamecube')
im,d=canvas();text(d,'Wii',35,35);end(im,'wii')
