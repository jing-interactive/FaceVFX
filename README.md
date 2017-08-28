# FaceVFX

Experimental project to showcase realtime face swap effects. 

Supported platforms

* Windows
* OSX
* iOS

Depends on

* [Cinder](https://github.com/cinder/Cinder) as the cross-platform creative coding framework.
* [Cinder-OpenCV3](https://github.com/cinder/Cinder-OpenCV3) for Computer Vision routines.
* [Cinder-VNM](https://github.com/vnm-interactive/Cinder-VNM) for my own utilities based on Cinder.
* Jason Saragih's FaceTracker library, self-contained in the project.
* [Cinder-ImGui](https://github.com/vnm-interactive/Cinder-ImGui) for cross-platform GUI.

Layout of the project and its dependency

    Cinder/
        blocks/
            Cinder-ImGui/
            Cinder-OpenCV3/
            Cinder-VNM/
        include/
        src/
    FaceVFX/


Tools used during development

* apitrace
* glintercept
* Very Sleepy

-------------------

### Photo mode (Tom Cruise)

![](https://raw.githubusercontent.com/OpenAVR/face-swapper/master/doc/tom-cruise.jpg)

### Movie mode

![](https://raw.githubusercontent.com/OpenAVR/face-swapper/master/doc/movie-mode.jpg)
