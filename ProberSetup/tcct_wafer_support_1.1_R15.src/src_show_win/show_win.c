/* 
   Agilent Technologies 2001
   Original Author: David Epperly
   
   Changes
   -------
   Modified 4/9/2001 by : Pierre Gauthier 
   
   The original version only allowed to iconify
   a window. This version allows for hiding and
   showing again the window
   
   ----------------------------------------------------

   this program allows a user to iconify, hide or show
   a window by specifying the option and the window name.
   
   Options:
   
   -hide <window name> :   hide the specified window
   -show <window name> :   show the specified window
   -iconify <window name> : iconify the specified window
   
   
   Example:  to hide the Smartest "Operation Control" window
   
   $show_win -hide "Control Window"
   
*/
   
    
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>     
     
#define MAX_WIN_AT_LEVEL 25000
     
bool findname(Display *dpy,
              Window top_win,
              char search_win_name[],
              Window win_found[],
              int win_found_max,
              int* nfound) 
{
  Window buf1[MAX_WIN_AT_LEVEL];
  Window buf2[MAX_WIN_AT_LEVEL];
  Window *parents;
  Window *children;
  Window *tmp;
  int nparents;
  int nchildren;
  int parent;
  int child;
     
  Window q_root_ret;
  Window q_parent_ret;
  Window *q_children_ret;
  unsigned int q_nchildren_ret;
  int q_child;
  int status;
  Atom actual_type;
  int actual_format;
  unsigned long nitems,leftover;
  unsigned char *data;
     
     
  parents = buf1;
  children = buf2;
  parents[0] = top_win;
  nparents = 1;
  *nfound = 0;
  while(nparents>0){
    /*check name*/
    for(parent=0;parent<nparents;parent++){
      status=XGetWindowProperty(
        dpy,parents[parent],XA_WM_NAME, 0L, 80L , False,
        XA_STRING, &actual_type, &actual_format, &nitems, &leftover, &data
      );
      if (status == Success){
        /* printf("name: <%s>\n", (char *) data); */
	if ((char *) data != NULL) {
          if (strcmp((char *) data,search_win_name) == 0 ){
             XFree(data);
             win_found[*nfound]=parents[parent]; 
             (*nfound)++;
             if (*nfound >= win_found_max){
               goto exit_true;
             }
          }
          XFree(data);
        }
      }else{
        printf("XGetWindowProperty failed. aborting.\n");
        exit(-1);
      }
    }
     
    /*build children list*/
    nchildren = 0;
    for(parent=0;parent<nparents;parent++){
      XQueryTree(
        dpy,parents[parent],
        &q_root_ret,&q_parent_ret,&q_children_ret,&q_nchildren_ret
      );
      if ( (nchildren + q_nchildren_ret) > MAX_WIN_AT_LEVEL){
        printf("MAX_WIN_AT_LEVEL (%d) exceeded. aborting.\n",MAX_WIN_AT_LEVEL);
        exit(-1);
      }
      for(q_child=0;q_child<q_nchildren_ret;q_child++){
        children[nchildren] = q_children_ret[q_child]; 
        nchildren++;
      }
      XFree(q_children_ret);
    }
     
    /*swap children to parents*/
    tmp = parents;
    parents = children;
    nparents = nchildren;
    children = tmp;
    nchildren = 0;
  } 
     
  if (*nfound > 0) goto exit_true;
     
exit_false:
  return False;
exit_true:
  return True;
}
     
void show_options() 
{

  fprintf(stderr,"wrong option!\n"); 
  fprintf(stderr,"Valid options are:\n"); 
  fprintf(stderr,"      -show     <window name>\n"); 
  fprintf(stderr,"      -hide     <window name>\n");
  fprintf(stderr,"      -iconify  <window name>\n"); 
  
}
     
int main (int argc, char * argv[])
{
  Display *dpy;
  Window win;
  int screen;
  Window *found_win;
  char *search_win_name;
  int status;
  int nfound;
  int nfound_max;
  int i;
  char option[64];
  
  if(argc<=1) {
    show_options();
    exit(-1);
  }   
     
  if ((dpy = XOpenDisplay(0)) == 0){
    fprintf (stderr, "cannot open X display server\n");
    exit (1);
  }
  screen = DefaultScreen(dpy);
  strcpy(option,argv[1]);
  search_win_name = argv[2]; 
  
  /* the parameter 1 is the maximum number of windows
     found */
     
  found_win = (Window *) calloc(1, sizeof(Window)); 

  if(findname(dpy,XDefaultRootWindow(dpy),search_win_name,found_win,1,&nfound)!= False)
  {
    printf("found %d window with name \"%s\"\n",nfound,search_win_name );
    for(i=0;i<nfound;i++){
 
      if(strcmp(option,"-iconify")==0) 
        status = XIconifyWindow(dpy, found_win[i], screen);  
      else if(strcmp(option,"-hide")==0) 
      {
        printf("  -hide option was selected...\n");
        status = XWithdrawWindow(dpy,found_win[i],screen); 
        printf("  Status of XWithdrawWindow() = %d\n",status);
      }
      else if(strcmp(option,"-show")==0) 
        status = XMapWindow(dpy,found_win[i]);
	  else if(strcmp(option,"-raise")==0) 
        status = XMapRaised(dpy,found_win[i]);
      else {
        show_options();
      }
    }
  }
  
  XCloseDisplay(dpy);

  return 0;

}
