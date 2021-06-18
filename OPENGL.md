## OpenGL Information

### The OpenGL features used in MineCraft, explained through example.

### Buffer Objects

There are many operations in OpenGL where you send a large block of data to OpenGL, such as passing vertex array data for processing. Transferring that data may be as simple as copying from your system’s memory down to your graphics card. However, because OpenGL was designed as a client- server model, any time that OpenGL needs data, it will have to be transferred from the client’s memory. If that data doesn’t change, or if the client and server reside on different computers (distributed rendering), that data transfer may be slow, or redundant.

Buffer objects were added to OpenGL Version 1.5 to allow an application to explicitly specify which data it would like to be stored in the graphics server.

Buffer Objects are OpenGL Objects that store an array of unformatted memory allocated by the OpenGL context (AKA the GPU). These can be used to store vertex data, pixel data retrieved from images or the framebuffer, and a variety of other things.

* Vertex data in arrays can be stored in server-side buffer objects
* Pixel data, such as texture maps or blocks of pixels, in buffer objects