# Coffeepesto

A clipboard manager for linux (X11 only) that

- is tiny, but supports a whole lot of features.
- supports all kinds of data - rich text, images, files in your file manager - you name it.
- has a nice gtk frontend
- has an impressive search feature (search by window, date, day, time)
  - you can even add custom data to the search index!
- has a configurable and potentially infinite history, regardless of the type of data
- can pin items
- can blacklist apps easily
- is modular and file-based so you can easily take parts of it and fit it into your existing setup (rofi, dmenu, anything) and is easy to script with

It only touches the clipboard (the one where you ctrl+c and right click copy), not the other X11 stuff.

## Demo

TODO: insert demo video

## How to get

### From the AUR
TODO: Publish both to AUR.

```bash
yay -S coffee-pesto # not there yet
```

### Manually

You will need the standard `xdotool` if you don't already have it. You can install it from your usual repositories.

First, install [this](https://github.com/porridgewithraisins/x11cp) xclip alternative. Why this requires an xclip alternative is explained on that page.

Then, install this program like so (linking with gtk+-3.0 libraries)
```bash
git clone https://github.com/porridgewithraisins/coffee-pesto

gcc `pkg-config --libs --cflags gtk+-3.0` -Wno-incompatible-pointer-types choose.c -o chooselisting

sudo cp clipdaemon chooselisting restorelisting /usr/local/bin
```

## How to use

- Run `clipdaemon` in the background by adding it to your autostart.
- Bind `chooselisting | xargs restorelisting` to a keyboard shortcut (such as super+v) and you will get a popup at your mouse for you to choose something from your history.
- In this popup, you can left click an item to select it, middle click an item to delete it, and right click to pin it.
- You can use the search bar to search for clipboard items. Out of the box, you can search by
  - the app you copied from, for all types of data
  - the date/day/time you copied it from (in `/usr/bin/date` format), for all types of data
  - (fuzzy) any part of the text itself, for textual data.

### Configure history size

It is 100 by default. Set the `COFFEE_PESTO_MAX_ITEMS` environment variable to change it.

### Blacklist windows

To blacklist windows, create a file (if it does not exist) at `~/.coffeepesto/blacklist`. In this file, write a _single_ line with a regex (extended grep) matching the names of windows you want to blacklist. For one, you can use this to prevent passwords copied from your password manager from entering your clipboard history, where it would be stored plaintext for anyone to read. You have to avoid partial matches yourself. It is case insensitive.

### To temporarily pause clipboard history

You just have to create a file `~/.coffeepesto/pause`. Remove that file to resume. No need to restart the background app or anything.

### Cross-device sync

Just sync the `~/.coffeepesto` directory with any existing file sync tool - rsync, dropbox, or have this directory on a shared server.

### Change base directory

You can change the default of `~/.coffeepesto` by setting the `COFFEE_PESTO_DIR` environment variable.

### If copying certain things doesn't work

I have ignored certain targets i.e types of data in order to reduce clutter. Most applications work just fine. Suppose you copy something and it doesn't show up in your history. Check if _all_ the available targets for the copied data (Copy it, and run `getcp TARGETS` to check) are being ignored in the `ignored_targets` variable in the `clipdaemon` script. If it is, you can define a `COFFEE_PESTO_IGNORED_TARGETS` environment variable to override it.

## Debugging

 - The `clipdaemon` tool takes a `-V` or `--verbose` argument that will make it log extensively to `~/.coffeepesto/log`
 - The `chooselisting` tool logs a few things to standard error. You can run it standalone from the terminal and see the logs.

## Hacking

### Custom search information

Callback style. You can create any executable at `~/.coffeepesto/custom-index`. It will be passed the directory containing all files related to the just-copied clipboard item as the 1st argument. See [here](#data-structures-used) to understand how to read the data structure.

As a random idea, you can make your clipboard entries searchable by whether you were at work or at home. You can query the current wifi network and use that to conditionally append `work` or `home` to the file.

This also means you can make this executable a sort of post-copy hook, if you want to do that, for... some reason.

### How it works

- `daemon` takes whatever is copied and stores it in ~/.coffeepesto/h folder. It also stores metadata to be used in search queries.
- `chooselisting` reads that folder and shows the listings. It puts to `stdout` the listing number you chose. It offers fuzzy search capabilities on the data and metadata stored by `daemon`.
- `restore` takes a listing number as argument, and puts the data at that listing number on the clipboard.

### How the x11 clipboard works

It is convoluted. There is no separate buffer called the "clipboard". Instead, the window you copy from, and the window you copy to, do _message passing_ to send the data you copy.

There is also such a thing as "targets", which is usually the type of data. For example, when you copy an image on google docs, it doesn't put the image on your clipboard, it instead puts an `<img src='some url'>` with the target as `text/html`, and when you ctrl+v, it requests the target `text/html` and then displays it as an image. If you copy a file in your file manager, it doesnt copy the whole file to your clipboard, obviously. It just copies the file path, sets the target as `file-list` or something along those lines. Then when you paste the file, it requests data as that target.

### Data structures used

```
  $ tree ~/.coffeepesto
  ~/.coffeepesto
  └── h
      ├── 1
      │   ├── text\html
      │   │   ├── data
      │   │   └── index
      │   └── text\plain
      │       ├── data
      │       └── index
      ├── 2
      │   ├── image\png
      │   │   ├── data
      │   │   └── index
      │   └── text\html
      │       ├── data
      │       └── index
      └── 3
          └── image\png
              ├── data
              └── index
```

Inside the `h` subfolder, each numeral-named folder corresponds to a different item ("listing").

Inside each such listing `$i`, there is one folder for each target. For each target, there is a data file and an index file. The data file has, well, the data itself. The index file contains metadata for search purposes. Additionally, for text-based data, the index file also contains a copy of the data. Metadata is duplicated for each target.

Since we are taking target names which are untrusted input and interpreting it as a filesystem entry, we have to reject a few target names. We only allow `[a-zA-Z-/]` in target names, and convert `/` to `\` before writing to the filesystem. This should cover all normal targets.

### How pinning works

Pinned items have the sign of their listing number negated. For example, listing number 3 gets renamed to -3. When you unpin an item, it gets reverted back to 3. In the GUI application, -3 is interpreted as an unsigned number so that it shows up as the "latest" item i.e it gets pinned.

### Additional files

- A `~/.coffeepesto/blacklist` file contains a single line extended grep regex which will be used to identify windows that are blacklisted. Copies from windows identified as such will not be saved into history.

- A `~/.coffeepesto/ourpaste` file is created by the `restorelisting` tool so that the item you restored doesn't go into the history again and is ignored by the daemon.

- While a file exists at `~/.coffeepesto/pause`, items will not be saved to clipboard history. It is used for temporarily pausing clipboard history.
