		---------------------------------------------------
			Readme file of Regshot 1.61d  2002/07/13
		---------------------------------------------------
			Please view whatsnew.txt for update info!!!

-----------------
Package includes:
-----------------
regshot.exe,language.ini,readme.txt,whatsnew.txt


-----------------
Introduction:
-----------------
RegShot is a small registry compare utility  that allows you to quickly take a  snapshot
of your registry and then compare it with a second one - done after doing system changes
or installing a new software product. The changes report can be produced in text or HTML
format and contains a list of all modifications that have taken place between  snapshot1
and snapshot2.In addition, you can also specify folders (with sub filders) to be scanned
for changes as well.In version 1.60+ you can save your whole registry in a *.hiv file for
future use.
Note: Regshot is a FREEWARE!

-----------------
Usage:
-----------------
(1)CLICK "1st shot" BUTTON
It pops up a menu which contains several items:
 (A)"Shot"  to take a snapshot only,and it will not be kept if you exit regshot program;
 (B)"Shot and save..." to take a snapshot of your registry and save the whole registry to
    a "hive" file and you can keep it in your harddisk for future use; NOTE:"hive" files
    are too big!
 (C)"Load..." to load a "hive" file previous saved.
If you want to monitor your file system ,just check the "Scan Dir [dir..]" checkbox
and input the folder names below it. Note: Regshot has the ability to scan multiple 
folders,Just separate them with ";",Regshot also scan the subfolders of the current 
folders you entered. 
Note:This version  only save your registry to "hive" file,it does NOT include the
folders you scaned!

(2)RUN SOME PROGRAMS which may change your windows registry,or it may change the file system

(3)CLICK "2nd shot" BUTTON

(4)Select your output LOG file type,"text" or "HTML,default is "text"

(5)INPUT YOUR COMMENT for this action into the "comment field",eg:"Changes made after
winzip started". COMMENT will only be saved into compare log files not into "hive" files

(6)CLICK "compare" BUTTON
Regshot will do the compare job now(auto detect which shot is newer),when it is finished,
Regshot will automatically load the compare LOG as you defined above,the log files are
saved  in the directory where "Output path" is defined,default is your Windows Temp Path 
,it was named as the "comment" you input,if the "comment field" is empty or invalid, the
LOG will be name as "~resxxxx.txt" or "~resxxxx.htm" where "xxxx" is 0000-9999.

(7)CLICK "Clear" BUTTON
You  will clear the two snapshots(or separately) previous made and begin a new job.
Note:"Clear" does not erase the log files!

(8)TO QUIT Regshot,just hit "Quit" button

(9)You can change the language of the regshot at main window,all words are saved in the 
file "language.ini". View it for details!

-----------------
Thanks:
-----------------
Special thanks:
Alexander Romanenko	-- Space provider! http://www.ist.md/

Toye			-- Release!
zhangl			-- Debug!
firstk			-- Debug!
mssoft			-- Test!
dreamtheater		-- Test!
Gonzalo			-- Spainish
ArLang°¢ÀÉ		-- Chinese
Mikhail A.Medvedev	-- Russian[Thanks!]
Kenneth Aarseth		-- Norsk
Marcel Drappier		-- French
Vittorio "Capoccione"	-- Italian
Gnatix			-- Deutsch
Murat KASABOGLU		-- Türkçe
Paul Lowagie		-- Nederlands
ondra			-- Èesky
AVE7			-- Fixed Deutsch!
Pau Bosch i Crespo	-- Catalan
Michael Papadakis	-- Greek
Per Bryldt		-- Danish
Rajah			-- Latvian
Leandro			-- Portuguese
Roland Turcan		-- Slovak
Nick Reid		-- Advice
Franck Uberto, Patrick Whitted, Walter Bergner, Jim McMahon, Fred Bailey,
Dchenka and all those I fogot to mention!!
------------------------------------------------------------------------------------
		Homepage:http://regshot.yeah.net/ http://regshot.ist.md
				Email:spring_w@163.com
			(c) Copyright 2000-2002 TiANWEi
				All rights reserved
