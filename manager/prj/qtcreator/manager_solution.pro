TEMPLATE = subdirs

rpi {
    MKFL = "Makefile.rpi"
} else {
    MKFL = "Makefile"
}


SUBDIRS += manager

manager.file = manager.pro
#manager.depends = qbase qdata q qmath def_lang
