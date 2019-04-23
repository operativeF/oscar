## Using Branches with git

Our practice is to do all development in branches rather than master.


### Why Use Branches?

Setting aside the religious wars that can arise over any development methodology, here are several advantages to doing all development in branches rather than master:

- master is always in a consistent state (and passes whatever tests your workflow requires). This means:
	- Anyone can branch from master at any time to start work on a feature or fix.
	- When someone is working on a feature or fix, they can update their branch with the current state of master (i.e. merge master into their branch) at any time before submitting a merge request.
- Merge conflicts, when they arise, are much easier to manage. For example:
	- Merge conflicts don't stop everyone in their tracks and block pushing work-in-progress to the repo. Instead, since everyone is working in their own branch, merge conflicts can deferred to a time of each author's choosing.
	- Merge conflicts are addressed within a branch by the branch author, who will best know how to resolve conflicts.
	- Merge conflicts are resolved before a branch is merged back into master, 
- Work-in-progress in its own branch can safely be pushed to the repo for review or testing by others.
- Branches let you follow a merge-request workflow that lets you document your changes and enforce your tests.
- Branches let you follow the same workflow for both core development and outside contributions.


### How Do I Develop in a Branch?

If necessary, fork your own copy of the repo.  Then:

1. Create a branch to work on your feature or bugfix:

        git clone https://gitlab.com/my-repo/OSCAR-code.git
        cd OSCAR-code
        git checkout -b my-branch

2. Write your code, committing to your branch as you go, using `git add` and `git commit -a`.

3. Test your code. See the instructions on how to build the project. It should build successfully, without warnings, and your feature should work as intended.

4. Sync your branch with master:

        git checkout master
        git pull
        git checkout my-branch
        git merge master

5. If there are any merge conflicts, resolve them and then build and test again. See below for details on resolving conflicts.

6. Push your branch up to gitlab: `git push -u origin my-branch`. Note that `git push` by itself won't push a branch that doesn't exist upstream.

7. Create a merge request describing your proposed change, linking to any issues that it might address, and attaching your branch.
	* You may need to repeat steps 2-6 a few times, if changes are needed before your request is accepted, or 4-6 if other changes are merged into master before your request is accepted.

8. Accept the merge request, which merges your branch into master and updates any linked issues.

If you have commit access to the repo, you *can* theoretically skip steps 7 and 8 and just merge your branch into master directly, but this is discouraged, as it may circumvent automated tests or other workflow requirements. But if you absolutely can't bear to deal with creating a merge request in a particular situation, at least do your development in a branch rather than master!


## Handling Merge Conflicts

People dread merge conflicts, but they're not actually all that frequent: most conflicts can be automatically resolved by git. On the other hand, this means that the conflicts that do arise require more attention, and the infrequency can lead to unfamiliarity with the process.

The cheat sheet for resolving merge conflicts is:

* Address each file with a conflict, then `git add` it.
* Check `git status` to see if there are any conflicts remaining.
* `git commit` when you've addressed all conflicts.
* Build and test the new code.

So what does it mean to address a file with a conflict?  Most merge conflicts fall into one of two categories:

1. A file was deleted in one branch and modified in the other.
2. Some lines in a file were changed in both branches, and the changes were different.

File deletions are easy:  If the file should remain, `git add` your copy back into your branch. If the file should be deleted, `git rm` it from your branch. The rest of this discussion will focus on conflicting changes in a file.


### What Just Happened?

In the case that files were changed in both branches, most of the fear and confusion comes from the fact that *even a failed merge modifies the files in your working directory,* leaving you with code that won't compile or run. So the first important command to remember is `git merge --abort`.

If you have a failed merge, and you just want to revert your attempt to merge, `git merge --abort` will revert your working directory back to your previous commit. (And if you get really stuck, you can `git reset --hard` to revert to your previous commit, but this will also throw away any changes in your working directory.)

But eventually you'll need to deal with the merge conflict. So the next step is to understand the state of your working directory after a failed merge:

* Files that were able to be merged successfully have been modified and marked ready for commit.
* Files with conflicting edits have been merged as much as possible, and conflicts have been marked as described below.
* **No change has yet been committed to the repository.**

Read that last point again and breathe a sigh of relief.

While this default result may seem surprising and scary, it is in fact very practical: it gets you as close to a resolved merge as it can, and provides you with much of the information you need to resolve the remaining conflicts. Once you resolve the conflicts, you mark the resolved files ready for commit, and then you'll be able to commit the merge.

### What Do I Do?

git will tell you which files had conflict when `git merge` fails. But you can ask it again with `git status`, which will list "Unmerged paths" that still need resolving. This is especially handy when you have multiple files to resolve, so you can check your progress and see if you've gotten them all.

Now let's examine what a merge conflict looks like by default. Here are the contents of a sample file with a merge conflict:

    preceding merged text
    <<<<<<< HEAD
    First file in master, modified by second-branch twice.
    =======
    First file in master, modified by first-branch.
    >>>>>>> master
    following merged text

Everything outside of a "<<<<<<<" and ">>>>>>>" was successfully merged. (Note that there may be multiple conflicts in a single file, so search for "<<<<" to make sure you've found them all.)

Everything above the "=======" is the text in HEAD (your branch). Everything below it belongs to master. So let's resolve the conflict:

    preceding merged text
    First file in master, modified by first-branch once and second-branch twice.
    following merged text

**In short, there's nothing magic about the conflict information git inserted into your file, it's just there to show you the two versions that are conflicting. All you have to do is replace the <<<<< through >>>>> lines with the text that should appear there.**

Once you've fixed the file, you can use `git diff` to make sure the changes are what you expected. (This will also help you catch any conflicts you missed.) For example:

    - First file in master, modified by second-branch twice.
     -First file in master, modified by first-branch.
    ++First file in master, modified by first-branch once and second-branch twice.

Once you're satisfied, you need to mark the file ready to commit, using `git add`. Again, `git status` will tell you if there are any more files with unresolved conflicts. (It also helpfully points you to `git add` to mark a file as resolved.)

And once all your conflicts are resolved, you complete the merge with `git commit`.

> **NOTE** the one gotcha in this process: if you start `git commit` and change your mind, and simply quit your editor as you normally would to abort a commit, **it will still commit** -- because your commit message isn't empty! (It gets pre-filled with a default merge comment.) While it's annoying to have a commit you didn't intend, the easiest course of action is just to fix anything with another commit. But *if* you can overcome your muscle memory before quitting the editor, you can delete the default merge comment and save it, and then git will abort the commit.


### Getting More Information About a Conflict

In the above example, git's default output provided all the information we needed to figure out what the merged text should be. But sometimes we're not sure. Thankfully, git can tell us more. The first command to know is:

`git config merge.conflictstyle diff3`

This tells git to mark merge conflicts with not just the conflicting text in your branch and in master, but also what that text originally was before your branch or master changed it. So, in this example, you would instead see:

    preceding merged text
    <<<<<<< HEAD
    First file in master, modified by second-branch twice.
    ||||||| merged common ancestors
    First file in master.
    =======
    First file in master, modified by first-branch.
    >>>>>>> master
    following merged text

In complicated conflicts, seeing the original state of the code can be incredibly helpful. Note that this setting needs to be configured before `git merge` tries to merge the conflicting files and inserts the conflict information into the file. But never fear: you can simply set do the `git config` above, `git merge --abort` and then attempt your `git merge` again to see this full context. 

And you might want to add the `--global` flag to git config so that you always see this on your projects.

The second command to know (which isn't as easy to remember) is:

`git log --merge -p [filename]`

This shows you all the commits and changes to the file that caused it to diverge. It's quite a bit more information than you usually need, but in those few instances you need more this is often just what you wanted to see.

And if you need to see *everything*, you can use `git show :1:filename` to see the entirety of the ancestor file, `:2:` to see the entirety of the file in your branch before the merge conflict, and `:3:` to see the entirety of the file as it currently exists in master.
