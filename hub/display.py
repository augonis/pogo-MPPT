#
# This file supports http://davesteele.github.io/development/2021/08/29/python-asyncio-curses/
#
from __future__ import annotations
import asyncio
from abc import ABC, abstractmethod
from curses import ERR, KEY_RESIZE, curs_set, wrapper
from textwrap import TextWrapper
import curses
from typing import Any, Coroutine


class Display(ABC):
    _inputTask = None
    
    
    def __init__(self):
        self.done: bool = False
        self.inputBuffer = asyncio.Queue()
    
    def _setScreen(self, stdscr: curses.window):
        self.stdscr = stdscr
        

    @abstractmethod
    def make_display(self) -> None:
        pass

    def set_exit(self) -> None:
        self.done = True
    
    
    async def run(self) -> None:
        curs_set(1)
        self.stdscr.nodelay(True)
        
        while not self.done:
            char = self.stdscr.getch()
            if char == ERR:
                await asyncio.sleep(0.03)
            elif char == KEY_RESIZE:
                self.make_display()
            else:
                #self.set_exit()
                self.inputBuffer.put_nowait(chr(char))
                #self.handle_char(char)
    
    
    def __enter__(self):
        self._setScreen(curses.initscr())
        
        curses.noecho()
        curses.cbreak()
        self.stdscr.keypad(1)
        try:
            curses.start_color()
        except:
            pass
        
        self._inputTask = asyncio.create_task(self.run())
        self.make_display()
        return self
    
    def __exit__(self, *args):
        self.stdscr.keypad(0)
        curses.echo()
        curses.nocbreak()
        curses.endwin()


class StatusConsoleDisplay(Display):
    consoleWindow: curses._CursesWindow
    
    def __init__(self):
        super().__init__()
        self._statusText = ''
        self._inputText = ''
        self.consoleText = []
    
    def _setScreen(self, stdscr: curses._CursesWindow):
        super()._setScreen(stdscr)
        self.consoleWindow: curses._CursesWindow = self.stdscr.subwin(5, 5, 0, 0)
        self.inputWindow = self.stdscr.subwin(5, 5, 5, 0)
        self.statusWindow = self.stdscr.subwin(5, 5, 10, 0)
    
    def make_display(self) -> None:
        
        maxy, maxx = self.stdscr.getmaxyx()
        self.stdscr.erase()
        
        self._consoleSize = maxy-2, maxx
        self.consoleWindow.resize(*self._consoleSize)
        self._refreshConsole()
        
        self.statusWindow.resize(1, maxx)
        self.statusWindow.mvwin(maxy-1, 0)
        self.statusWindow.mvderwin(maxy-1, 0)
        self.statusWindow.addstr(0, 0, self._statusText)
        
        self.inputWindow.resize(1, maxx)
        self.inputWindow.mvwin(maxy-2, 0)
        self.inputWindow.mvderwin(maxy-2, 0)
        self.inputWindow.addstr(0, 0, self._inputText)
        
        self.inputWindow.refresh()
        self.consoleWindow.refresh()
        self.statusWindow.refresh()
        self.stdscr.refresh()
    
    def _refreshConsole(self):
        wrapper = TextWrapper(width=self._consoleSize[1]-1, subsequent_indent='  ')
        maxLines = self._consoleSize[0]
        lines = []
        for entry in reversed(self.consoleText):
            lines.extend(reversed(wrapper.wrap(entry)))
            if len(lines) > maxLines: break
        lines = lines[:maxLines]
        
        self.consoleWindow.erase()
        self.consoleWindow.addstr(0, 0, '\n'.join(reversed(lines)))
        self.consoleWindow.refresh()
    
    async def input(self, prompt):
        # clear inputBuffer
        while not self.inputBuffer.empty():
            self.inputBuffer.get_nowait()
        
        self._inputText = prompt+' '
        self.inputWindow.erase()
        self.inputWindow.addstr(0, 0, prompt)
        self.inputWindow.addch(' ')
        self.inputWindow.refresh()
        
        result = []
        while 1:
            ch = await self.inputBuffer.get()
            if ch == '\n':
                self._inputText = ''
                self.inputWindow.clear()
                self.inputWindow.refresh()
                return ''.join(result)
            self.inputWindow.addch(ch)
            self._inputText += ch
            self.inputWindow.refresh()
            result.append(ch)
    
    def print(self, message):
        self.consoleText.append(str(message))
        self._refreshConsole()
    
    def setStatus(self, message):
        self._statusText = message.strip()
        self.statusWindow.erase()
        self.statusWindow.addstr(0, 0, self._statusText)
        self.statusWindow.refresh()


async def filler(display):
    for i in range(100):
        display.print(f'Been {i} times'*i)
        await asyncio.sleep(0.5)


async def display_main():
    with StatusConsoleDisplay() as display:
        
        fillTask = asyncio.create_task(filler(display))
        
        display.setStatus('status is ok')
        display.print('Hello!')
        display.print('Workd!')
        await asyncio.sleep(2)
        
        display.print(await display.input('First question: '))
        display.print('Weeeee!')
        display.print(await display.input('Answer: '))
        
        await asyncio.sleep(2)
        display.set_exit()

if __name__ == "__main__":
    try:
        asyncio.run(display_main())
    except KeyboardInterrupt:
        pass