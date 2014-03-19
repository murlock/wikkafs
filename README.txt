
AIM: Provide a textual backend using FUSE to WikkaWiki


First draft : 
	Each Page will be created as directory in /AllPage (or /PageIndex ?)
	Each page will be shown as a list of revision ( r01, r002, ... )
	Each page will have a directory named attachment to add/remove binaries (only if we have access to target filesystem)
	/HomePage is a symbolic link to /AllPage/HomePage

Second draft:
	Each Page will be present in /
	Each Page with revisions will be shown as hidden directory .{PageName} and inside all revisions will be shown
	
