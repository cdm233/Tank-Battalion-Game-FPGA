# Tank-Battalion-Game-FPGA

This is a Tank Battalion game that runs on a DE1-Soc FPGA board written purely in C.

This game is a pvp game where two players battle on a maze like map. The first player to empty the other's HP wins.

This game can be run either on the actual board or on https://cpulator.01xz.net/?sys=arm-de1soc
![image](https://user-images.githubusercontent.com/80089456/167228357-3895b4ed-432e-437e-ac1b-baae8f127afa.png)

# Game Physics Engine
The game includes a simple physics engine that's written by me. The physics engine handles all game object collision, movement, and rendering.
It prevents bullets and players from going through a wall or bullets goes through a player without doing damage.

# Game Screen Shot
Game running:
![image](https://user-images.githubusercontent.com/80089456/167228404-5f1ab135-0aa0-4eee-96de-69afa70a38b9.png)

Game paused:
![image](https://user-images.githubusercontent.com/80089456/167228418-655f3dfa-2b21-46c7-9bb4-154b90ac9d77.png)

Player 2 won:
![image](https://user-images.githubusercontent.com/80089456/167228473-eee17d1c-888c-49c2-846a-1c1235dabbfd.png)

# Reference
The game graphics is create by Iris Zuo, which includes the tank, wall, and text that appears on screen.
