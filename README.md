### Core Utils 

This started out as a repo for a failed project in rust that I abandoned. 
I don't like having a ton of stupid little failed repos in my github account 
because it looks messy and makes me feel gross. For a while, I've been reusing
old repos for new projects.  This repo stood as sort of a miscellaneous place 
where I would throw up code that wasn't really in a serious project.  For 
instance, I had a little implementation of malloc in here, and some 
other random stuff.  

Some of these random projects were re-implementations of some of the coreutils.
Basically, all my other projects were a little bit too difficult and I was 
feeling a little burnt out, so I decided to do something easy and implement 
`cat` and `head`.  

When I went to repurpose this repo once again, I felt bad deleting the code 
here.  As such, I decided that I would implement even more of the coreutils 
and keep them here.  

All the core utils here are based on the MacOS / FreeBSD implementations
because I wrote them on a mac and I could easily cross reference the behavior. 
On the whole, I try my best to match the behavior of the original utils, 
however, on some of the more annoying ones (like `touch`), I simplify down
to just the basic functionality so that I learn how the util works without
getting bogged down in all the edge cases.

### TODO

- [x] cat
- [x] head
- [x] pwd
- [x] echo
- [x] yes
- [ ] touch
- [ ] mv
- [ ] cp
- [ ] rm
- [ ] tail
- [ ] ls
- [ ] mkdir
- [ ] rmdir
- [ ] more
- [ ] less
