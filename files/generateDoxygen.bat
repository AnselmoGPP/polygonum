@ECHO OFF

CD ..\..\_BUILD\projects\Renderer
ECHO Directory: 
CD

::Generate template config file:
::"C:\Program Files\doxygen\bin\doxygen" -g Doxyfile
::Update config file:
::"C:\Program Files\doxygen\bin\doxygen" -u Doxyfile
::Generate documentation from a config file:
"C:\Program Files\doxygen\bin\doxygen" ..\..\..\projects\Renderer\Doxyfile

PAUSE
