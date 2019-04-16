#! /bin/bash
#
mv /opt/tempDir /opt/OSCAR
ln -s /opt/OSCAR/OSCAR /usr/bin/OSCAR
cp /opt/OSCAR/OSCAR.desktop /home/$SUDO_USER/Desktop
chown $SUDO_USER:$SUDO_USER /home/$SUDO_USER/Desktop/OSCAR.desktop
chmod a+x /home/$SUDO_USER/Desktop/OSCAR.desktop
#
