
# MapSync

[![Preview video](https://img.youtube.com/vi/4ljONQutUi8/0.jpg)](https://www.youtube.com/watch?v=4ljONQutUi8)

This plugin synchronizes in-editor editing, in real time. As an example, if you move, or create a cube, it will move in your friend's editors too. This enables you to work faster, and to really collaborate when making a map. Once you've tried it, you will never go back again !

The plugin uses a client-server model ; the server is provided with the plugin, and it runs inside the editor - no third party server requiered !

Features:
- Synchronizes all actor's location, rotation and scale
- Synchronizes creating or destroying actors
- Supports static mesh actors' mesh and material
- Supports light actors' intensity and color

Technical information:
- One editor module
- Uses TCP sockets
- Based on map name (works even if project is not the same)
- Supports actor transform (location, rotation, scale)
- Supports creating and deleting actors
- Supports StaticMeshActor's mesh and material

To add new supported classes, just create a custom class, and add the functions that serializes the changes you want to replicate.
