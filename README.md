# Macro Interpreter
* Macro Interpreter is a project that can simulate the mouse/keyboard action with script.
## Features
### Keycode Mode
* This program can simulate the input by using both **virtual keycode** and **scancode** mode. Which allow the action can be caught by the directx game.
### Trigger Key / Loop Toggle
* The user can specify the `X1`(side button of the mouse), `X2`(side button of the mouse), `F1`~`F12` to trigger the script or use `ScrollLock` to run the script in loop mode.
* You also can long-press the trigger key, it will act as in loop mode.
### Focus Window Detection
* This function will prevent you from switching the window while running the script.
* Such that when you are running the script in a game application window, and you forget that the script is running and switch to explorer. Then the script may mess up your files.
### Very High Precision Sleep Function
* This program use [Query Performance Counter](https://docs.microsoft.com/en-us/windows/win32/api/profileapi/nf-profileapi-queryperformancecounter) as delay.
* QPC is a very high precision sleep function with error time ≤ 0.05ms.
* E.g. for a `pause(100)` call
    * **CPU usage** ≤ 0.2%
    * **Maximum error time** ≤ 35us
    * **Average error time** over 100 runs ≤ 1us
### Different Script Style for Different Scenario
* There are two style of script.
    * Main only Script
        * This is the suitable script style for most cases.
        * <img src="https://user-images.githubusercontent.com/12500621/163141464-abec0f3e-74c2-4089-ad0e-11c371b93528.png" width="300">
    * 3 Step Script
        * With 3 Step Script, you can specify `pre-block`, `main-block` and `post-block`. In 3-step macro, pre will runs prior to the main one. After main end or stop, the post one will run.
        * The behavior when trigger once is the same as Main only Script. But are **different** when in **loop mode**.
        * <img src="https://user-images.githubusercontent.com/12500621/163141549-e569d7c3-fbe5-45ef-8d08-11854f0f7123.png" width="450">
## Script Definition
### Special Keyword / Global Variable
* If **debug** is specified. It will also runs the getcursor mode. **Cursor pos**, **pressed key** and **sleep_api_time_resolution(TRes)** will be shown in title.
* If **directx** is specified. It will simulate all the input action with scancode, otherwise the virtual keycode will be used.
* If **scrollLockDisable** is specified. It will disable scrolllock as loop mode trigger.
* If **verboseMode** is specified. It will display all the input it sent.
* Targetkey is the trigger key to run the script. You can use `X1`, `X2` and `F1`~`F12` to trigger the script.
* Latency is the delay time which will be added between each input action.
### Script Layout (Main only)
```
[debug]?
[directx]?
[scrollLockDisable]?
[verboseMode]?
[targetKey]?
[latency]
[main]+
```
### Script Layout (3-step)
```
[debug]?
[directx]?
[scrollLockDisable]?
[verboseMode]?
[targetKey]?
[latency]
[pre]*
%
[main]+
%
[post]*
```
### Script Actions / Command
* Debug
    * "debug"
* directx
    * "directx"
* scrollLockDisable
    * "scrdis"
* verboseMode
    * "verbose"
* targetKey
    * "F1"
    * ...
    * "F12"
    * "X1"
    * "X2"
* latency
    * Non-negative integer number
* lines
    * click \[key|vk\]
    * press \[key|vk\]
    * release \[key|vk\]
    * mouse \[L|R\]\[0|1\]?//click if no 0/1, press if 0, release if 1.
    * mgoto \[x\] \[y\]
    * mgotor \[x\] \[y\]//relative
    * pause \[ms\]
    * system \[cmd\]
### Action Explain
* click \[key|vk\]
    * click \[key|vk\]
    * e.g. `click z`, `click 65`
* press \[key|vk\]
    * the same as click but only press
    * e.g. `press x`
* release \[key|vk\]
    * the same as click but only release
    * e.g. `press i`
* mouse \[L|R\]\[0|1\]?
    * click/press/relase the mouse
    * L/R specify left key or right key.
    * click if no 0/1, press if 0, release if 1.
    * e.g. `mouse L`, `mouse R0`
* mgoto \[x\] \[y\]
    * move mouse arrow to x,y absolute coordinate.
    * e.g. `mgoto 1920 1080`
* mgotor \[x\] \[y\]
    * move mouse arrow with x,y relative coordinate.
    * e.g. `mgotor 200 250`
* pause \[ms\]
    * pause time in millisecond
    * e.g. `pause 35`
* system \[cmd\]
    * run the system command
    * e.g. `system "another_program.exe"`
### Script Examples
* Example 1
```
50
click f
```
* Example 2:
```
F2
50
press a
%
click t
%
release a
```
* Example 3:
```
directx
F3
20
mouse L
```
* Example 4:
```
directx
20
click p
%
click c
%
```
* Example 5:
```
debug
directx
scrdis
X2
30
click p
click i
```
## How to use
* The program take parameters as script file path. So you can start the script by issue the `macro-interpreter.exe script1.txt script2.txt` or just drag&drop the script file onto the `macro-interpreter.exe`.
* The program will use `macro.txt` as default script path if not provide.
## Reference
* [Scancode](http://download.microsoft.com/download/1/6/1/161ba512-40e2-4cc9-843a-923143f3456c/scancode.doc)
* [Virtual Keycode](https://docs.microsoft.com/zh-tw/windows/win32/inputdev/virtual-key-codes)
