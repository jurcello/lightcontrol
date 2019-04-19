# About Photonic Director

Photonic Director is a program for controlling lights and more general DMX
controlled fixtures. It's first aim was to offer sound based controlling of
lights using specialized effects but has now extended to allow for any
time based data that can be send using the OSC protocol.
Although many programs allow to do this, Photonic Director also makes heavy
use of the position of lights where the position doesn't need to be in a grid:
many effects use the 3d position to calculate the intensity of a given light.

The development is primarily driven by my own needs when creating a 'light
composition' to my music. That means that it's features might not always
of big use for other purposes and that the user interaction is entirely
created to allow myself to work with it quickly.

The only DMX device that is currently supported is the Enttec DMX Usb pro.

## About the code

The secondary reason to create this project was to learn to code c++.
This means that I tried several approaches to certain problems which means
there might be inconsistencies in the code (for instance the use of raw pointers
vs shared pointers). I might refactor these inconsistencies when needed,
but for now I will not do these refactorings when there is no absolute need
(that is random crashes during shows).

