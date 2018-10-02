When you run ../update_dependencies.bat:

* The Externals directory will be populated with the Falcor dependencies
* The ../Framework/Media directory will be created with a link

If you move the whole code tree somewhere, the links will turn into "real" directories and cause build problems. Run delete_dependencies.bat to delete these and start afresh.