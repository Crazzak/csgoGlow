# Project csgoGlow, a 100% FREE and OPEN-SOURCE wallhack for OSX

**Thanks to @gabsens and @e-retailer’s contribution to this project**

## Usage: 
	sudo ./Wall [-f <max flash alpha>] [-r <refresh rate>] [-a <glow alpha>] [-s] [-t] [-u] [-o] [-h]
	
	-f <flash alpha>	: Antiflash alpha max amount (default: -1, disable: -1, range: [0-2700])
	-r <refresh rate>	: Refresh rate in microseconds (default: 10000.0)
	-a <glow alpha>		: Glow alpha (default 0.5, range: [0-1])
	-s			: Enables Spotted on Radar
	-t			: Enable teammate glow
	-u			: Enable weapons/utility/bomb/chicken glow
	-o			: Get new offsets (only use with -insecure launch option flag in CSGO)
	-h			: Display this message
	
	Note:
	1) -o is currently unavailable
	2) use sv_dump_class_info to find more entity classes
## Example:
	sudo ./Wall -f 0 -r 10000 -a 0.5 -s -t -u
## Terminate:
	Type "stop" or "exit" or "quit" or "q" and press the Return key or terminate csgo
## Rank Reveal:
	Type "ranks" and press the Return key
		- use command-K to clear the screen

### Hello World
if you are newbie and have never use Xcode, go and watch @gabsens’s [Video at Youtube](https://www.youtube.com/watch?v=AefIiBg2HKU)

don't get scammed by people trying to make money off of this hack!

### How to run?
clone it from github and run it with **ROOT**.

### What if CS:GO updates?
Nothing. Since there is pattern scanning implemented you don't need to update any Offset-Pointers.

### Is it safe?
Maybe, depends on Gaben

### More features?
No! We won't implement aimbot, bunnyhop or other stuff to it. This is just a native CS:GO Wallhack.

But you can refer to https://csgoglow.hipchat.com/invite/646884/2885f39b8a11953a3039070fe8325713 signup and ask questions about other features. The issue Tracker should be an Issue tracker.

## About `Internal`:
sorry for another delay, there some new trouble.

## Licenses
[AGPLv3](https://www.gnu.org/licenses/agpl-3.0.html)
![](https://camo.githubusercontent.com/cab2b2b8ca7a05fdb59e6ded7fd4645f3a23ab7f/68747470733a2f2f7777772e676e752e6f72672f67726170686963732f6167706c76332d3135357835312e706e67)

We believe that you know what you are doing. You should get this software for free.
请确定你在免费试用这个外挂！
