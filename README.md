# mookFilter~
A digital approximation of the Antti's Moog Filter implemented in a Max external object.  
Original code from ed.luosfosruoivas@naitsirhC and [musicdsp.org](http://musicdsp.org) community.

## How to install the external
- Drag and drop `/mookFilter` folder into your Max8 Packages folder.

### Requirements
Max7 or Max8 must be installed on your computer.

## How to build a new one
- Clone the repository.
- Drag and drop the `/build` folder content into a new folder `/mookFilter~`.  
- Drag and drop the newly created folder into your `/max-sdk-8.2/source/audio/` folder.
- Launch Terminal
- `cd [your path to the SDK]/max-sdk-8.2/source/audio/mookFilter~`
- `cmake -G Xcode`
