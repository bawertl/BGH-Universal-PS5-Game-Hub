# Authored BGH console silhouettes. SPDX-License-Identifier: MIT
from PIL import Image,ImageDraw
from pathlib import Path
out=Path(__file__).resolve().parents[1]/'assets'/'platforms'
out.mkdir(parents=True,exist_ok=True)
for kind in ['ps5','ps4','ps2','switch','xbox360']:
 im=Image.new('RGBA',(256,256));d=ImageDraw.Draw(im);white='#EDF4FF';dark='#101B2D';mid='#90A9C7'
 if kind=='ps5':
  d.polygon([(108,32),(148,29),(150,220),(100,220)],fill=dark)
  d.polygon([(65,18),(109,33),(102,214),(79,232),(76,88)],fill=white)
  d.polygon([(151,29),(183,18),(177,175),(192,221),(151,219)],fill=white)
  d.line([(107,43),(103,205)],fill='#61BDFF',width=5);d.ellipse((64,217,195,238),fill=mid)
 elif kind=='ps4':
  d.polygon([(27,75),(191,58),(229,98),(65,117)],fill=white)
  d.polygon([(65,125),(229,106),(229,139),(65,159),(27,119),(27,85)],fill=mid)
  d.polygon([(28,128),(65,168),(229,148),(229,177),(65,197),(28,158)],fill=white)
  d.line([(144,67),(174,105),(174,184)],fill='#75BFFF',width=4)
 elif kind=='ps2':
  d.polygon([(61,37),(153,24),(193,50),(193,220),(99,233),(61,205)],fill=mid)
  d.polygon([(61,37),(99,65),(99,233),(61,205)],fill=dark)
  d.polygon([(99,65),(193,50),(193,220),(99,233)],fill=white)
  for y in range(118,207,12):d.line([(110,y),(184,y-10)],fill=dark,width=4)
  d.line([(116,77),(176,69)],fill='#3188FF',width=7)
 elif kind=='switch':
  d.rounded_rectangle((18,55,238,201),radius=29,fill=white)
  d.rectangle((65,55,190,201),fill=dark);d.rounded_rectangle((74,66,181,190),radius=8,fill=mid)
  d.ellipse((28,78,50,100),fill=dark);d.ellipse((205,155,227,177),fill=dark)
  for x,y in [(30,139),(46,139),(38,131),(38,147),(205,91),(221,91),(213,83),(213,99)]:d.ellipse((x-3,y-3,x+3,y+3),fill=dark)
 elif kind=='xbox360':
  d.polygon([(69,27),(174,27),(163,82),(158,129),(163,178),(174,228),(69,228),(78,176),(83,127),(78,78)],fill=white)
  d.rounded_rectangle((93,42,146,110),radius=9,fill=mid);d.line((102,55,137,55),fill=dark,width=3)
  d.ellipse((107,139,144,176),fill='#70BA6B');d.ellipse((116,148,135,167),fill=white)
  d.rounded_rectangle((107,193,144,209),radius=3,fill=dark)
 im.resize((96,96),Image.Resampling.LANCZOS).save(out/(kind+'.png'))
