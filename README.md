# rm_electrical-supercap_v2

So i heard that having a readme is good and professional, so if anyone bothers to read this and is curious:
    The objective of the project is to use the excess allowed power draw. We chose the 90W at the time of writing I believe.
    Started out with a half bridge topology for the buck boose to/from 24V:3V supercaps.
    Apparently many compromises were made because Jed (Ged?) didn't want to use a microcontroller.
    So now we're at full bridge, using mspm0, with a lot of sensors and stuff so that zero voltage switching can be implemented to avoid power loss spike
    This specific branch, is a redesign to minimize powerpath (mainly), since we have more depth looking into the robot in our little compartment of space.