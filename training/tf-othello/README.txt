To run the training script you need to run the script parse.py in python 3.7 with tensorflow 1.15.5 installed, parameters are:

-number of blocks, mandatory
-number of channels, mandatory
-prefix of the training blocks path
-restore file prefix

ex:
python parse.py 4 32 "C:\Users\simon\OneDrive\Desktop\Appunti\Tirocinio Alpha Zero Othello\leela-zero-othello\training\tf-othello\Training\tmp"
