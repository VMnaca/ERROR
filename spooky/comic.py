stream = []
import cv2
import os
import FFT
import glob
from pydub import AudioSegment as AS
from py_sonicvisualiser import SVEnv
import sys
import wave
import numpy as np
#from C:\Users\15126\Desktop\Python\py_sonicvisualiser-master\py_sonicvisualiser import SVEnv
def function(stream):
    x=0
    output = []
    #Binary number as an integer in base 2
    for i in stream:
        binary_int = (int(stream[x].replace (' ', ''), 2)) #May be able to get rid of replace depending on how the data comes in 
        x+=1

        #Turns number into array of bytes and then decides how to read it
        binary_array = binary_int.to_bytes((binary_int.bit_length() + 7)//8, "big")

        #Turns the binary bytes into ascii
        ascii_text = binary_array.decode()
        output.append(ascii_text)
    return output

files = glob.glob(os.path.join('Images',"*.jpg"))
for f in files:
    os.remove(f)

files = glob.glob(os.path.join('WAV_Output',"*.wav"))
for u in files:
    os.remove(u)

vidcap = cv2.VideoCapture(r'Videos\anger_gif.mp4')

def nothing(x):
    pass

def getFrame(sec):
    vidcap.set(cv2.CAP_PROP_POS_MSEC,sec*1000)
    hasFrames,image = vidcap.read()

    low_thresholds = [50,50,50]
    high_thresholds = [200,200,200]
    if hasFrames:
        gray_img = cv2.cvtColor(image, cv2.COLOR_BGR2GRAY)
        imageThresh = cv2.adaptiveThreshold(src=gray_img, maxValue=255, adaptiveMethod=cv2.ADAPTIVE_THRESH_MEAN_C, thresholdType=cv2.THRESH_BINARY, blockSize=101, C=-10)

        

        #cv2.imshow("filtered", imageThresh)

        #cv2.waitKey(0)
                   
                
        
        cv2.imwrite(r"Images\image"+str(count)+".jpg", imageThresh)
    return hasFrames
sec = 0
frameRate = 0.5
count=1
success = getFrame(sec)
while success:
    count = count + 1
    sec = sec + frameRate
    sec = round(sec,2)
    success = getFrame(sec)


def render_image():
    files = glob.glob(os.path.join('Images',"*.jpg"))
    
    #Turns .jpeg into .wav
    x=0
    for file in files:
        output = 'WAV_Output\spectro_'+str(x)+'.wav'
        FFT.main(files[x],output)
        x+=1    


def wav_comic():
    y=0
    combined_sounds = AS.empty()
    files = glob.glob(os.path.join('WAV_Output',"*.wav"))
    for file in files:
        sound1 = AS.from_wav(r"WAV_Output\spectro_"+str(y)+".wav")
        combined_sounds = combined_sounds + sound1
        y+=1
    combined_sounds.export("final_solution.wav", format="wav")
    wave.open("final_solution.wav", 'rb')
    sve = SVEnv(5,wave.open("final_solution.wav", 'rb').getnframes(),"final_solution.wav")
    #specview = sve.add_spectrogram()
    #os.system("sonicvisualiser.exe " + "final_solution.wav")

        
#print(round(vidcap.get(cv2.CAP_PROP_FRAME_COUNT)/vidcap.get(cv2.CAP_PROP_FPS)))    

getFrame(round(vidcap.get(cv2.CAP_PROP_FRAME_COUNT)/vidcap.get(cv2.CAP_PROP_FPS)))        
render_image()
wav_comic()
    
