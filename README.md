File unpacker for D3GR and WAV files from Sanitarium.
D3GR is a graphic resource file type, the program will extract sprites included in the RES files from the game.
I've abandoned this project, but I'm making it public so that if at some point anyone is trying to do the same thing, you can use my findings on this file format to do so.

The limitation of my program so far is that each RES file has its own palette and finding each one through memory analysis was very taxing. 
TO DO would be to figure out if there's a specific file where all palettes are stored.
Or just get them out manually loading each individual world and create a mapping for each RES file in the game. Each world has
its own palette and from what I got, each RES file corresponds to a specific world/screen.
