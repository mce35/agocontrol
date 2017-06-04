#!/bin/bash

# set up symlinks .in.html -> .xml.in
find -name \*.in.html -execdir bash \-c "file=\"{}\";ln -s \"{}\" \"\${file%.in.html}.xml.in\"" \;

# dynamically create POTFILES.in
find -name \*.xml.in -printf "%P\n" > po/POTFILES.in

# update translation files
cd po
intltool-update --pot --verbose --gettext-package=agocontrol
for i in *.po
do
    echo -n "Updating $i "
    msgmerge --update "$i" agocontrol.pot;
done
cd ..

# remove temporary files
find -name \*.xml.in -delete
rm po/POTFILES.in

