import wave, struct, math # To calculate the WAV file content
import numpy as np # To handle matrices
from PIL import Image, ImageOps # To open the input image and convert it to grayscale


import scipy.ndimage # To resample using nearest neighbour

'''
    Loads a picture, converts it to greyscale, then to numpy array, normalise it so that the max value is 1 
    the min is 0, increase the contrast a bit, remove every pixel which intensity is lower that 0.5, 
    then resize the picture using nearest neighbour resampling and outputs the numpy matrix.
    
    FYI: imgArr[0,0] is the top left corner of the image, cheers matrix indexing
    
    Returns: the resized image as a high contrast, normalised between 0 and 1, numpy matrix
'''
def loadPicture(size, file, contrast=True, highpass=False, verbose=1):
    img = Image.open(file)
    img = ImageOps.expand(img,border=60,fill='black')
    img = ImageOps.expand(img,border=15,fill='white')
    img = ImageOps.expand(img,border=1,fill='black')
    #img_with_border = ImageOps.expand(img,border=300,fill='black')
    img = img.convert("L")
    #img = img.resize(size) # DO NOT DO THAT OR THE PC WILL CRASH
    
    imgArr = np.array(img)
    imgArr = np.flip(imgArr, axis=0)
    if verbose:
        print("Image original size: ", imgArr.shape)
        
    # Increase the contrast of the image
    if contrast:
        imgArr = 1/(imgArr+10**15.2) # Now only god knows how this works but it does
    else:
        imgArr = 1 - imgArr
    # Scale between 0 and 1
    imgArr -= np.min(imgArr)
    imgArr = imgArr/np.max(imgArr)
    # Remove low pixel values (highpass filter)
    if highpass:
        removeLowValues = np.vectorize(lambda x: x if x > 0.5 else 0, otypes=[np.float])
        imgArr = removeLowValues(imgArr)

    if size[0] == 0:
        size = imgArr.shape[0], size[1]
    if size[1] == 0:
        size = size[0], imgArr.shape[1]
    resamplingFactor = size[0]/imgArr.shape[0], size[1]/imgArr.shape[1]
    if resamplingFactor[0] == 0:
        resamplingFactor = 1, resamplingFactor[1]
    if resamplingFactor[1] == 0:
        resamplingFactor = resamplingFactor[0], 1
    
    # Order : 0=nearestNeighbour, 1:bilinear, 2:cubic etc...
    imgArr = scipy.ndimage.zoom(imgArr, resamplingFactor, order=0)
    
    if verbose:
        print("Resampling factor", resamplingFactor)
        print("Image resized :", imgArr.shape)
        print("Max intensity: ", np.max(imgArr))
        print("Min intensity: ", np.min(imgArr))
    return imgArr

def genSoundFromImage(file, output="sound.wav", duration=5.0, sampleRate=44100.0, intensityFactor=1, min_freq=0, max_freq=22000, invert=False, contrast=True, highpass=True, verbose=False):
    wavef = wave.open(output,'w')
    wavef.setnchannels(1) # mono
    wavef.setsampwidth(2) 
    wavef.setframerate(sampleRate)
    
    max_frame = int(duration * sampleRate)
    max_intensity = 32767 # Defined by WAV
    
    stepSize = 400 # Hz, each pixel's portion of the spectrum
    steppingSpectrum = int((max_freq-min_freq)/stepSize)
    
    imgMat = loadPicture(size=(steppingSpectrum, max_frame), file=file, contrast=contrast, highpass=highpass, verbose=verbose)
    if invert:
        imgMat = 1 - imgMat
    imgMat *= intensityFactor # To lower/increase the image overall intensity
    imgMat *= max_intensity # To scale it to max WAV audio intensity
    if verbose:
        print("Input: ", file)
        print("Duration (in seconds): ", duration)
        print("Sample rate: ", sampleRate)
        print("Computing each soundframe sum value..")
    for frame in range(max_frame):
        if frame % 60 == 0: # Only print once in a while
            print("Progress: ==> {:.2%}".format(frame/max_frame), end="\r")
        signalValue, count = 0, 0
        for step in range(steppingSpectrum):
            intensity = imgMat[step, frame]
            if intensity < 0.1*intensityFactor:
                continue
            # nextFreq is less than currentFreq
            currentFreq = (step * stepSize) + min_freq
            nextFreq = ((step+1) * stepSize) + min_freq
            if nextFreq - min_freq > max_freq: # If we're at the end of the spectrum
                nextFreq = max_freq
            for freq in range(currentFreq, nextFreq, 1000): # substep of 1000 Hz is good
                signalValue += intensity*math.cos(freq * 2 * math.pi * float(frame) / float(sampleRate))
                count += 1
        if count == 0: count = 1
        signalValue /= count
        
        data = struct.pack('<h', int(signalValue))
        wavef.writeframesraw( data )
        
    wavef.writeframes(''.encode())
    wavef.close()
    print("\nProgress: ==> 100%")
    if verbose:
        print("Output: ", output)

import sys
import argparse


def main(file, output):

    genSoundFromImage(
            file=file, 
            output=output)

if __name__ == "__main__":
    main(sys.argv[1:])
