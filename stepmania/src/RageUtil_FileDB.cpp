#include "global.h"

#include "RageUtil_FileDB.h"
#include "RageUtil.h"
#include "RageLog.h"
#include "arch/arch.h"

/* Search for "beginning*containing*ending". */
void FileSet::GetFilesMatching(const CString &beginning, const CString &containing, const CString &ending, vector<CString> &out, bool bOnlyDirs) const
{
	/* "files" is a case-insensitive mapping, by filename.  Use lower_bound to figure
	 * out where to start. */
	set<File>::const_iterator i = files.lower_bound( File(beginning) );
	for( ; i != files.end(); ++i)
	{
		if(bOnlyDirs && !i->dir) continue;

		/* Check beginning. Once we hit a filename that no longer matches beginning,
		 * we're past all possible matches in the sort, so stop. */
		if(beginning.size() > i->name.size()) break; /* can't start with it */
		if(strnicmp(i->name, beginning, beginning.size())) break; /* doesn't start with it */

		/* Position the end starts on: */
		int end_pos = int(i->name.size())-int(ending.size());

		/* Check end. */
		if(end_pos < 0) continue; /* can't end with it */
		if( stricmp(i->name.c_str()+end_pos, ending) ) continue; /* doesn't end with it */

		/* Check containing.  Do this last, since it's the slowest (substring
		 * search instead of string match). */
		if(containing.size())
		{
			unsigned pos = i->name.find(containing, beginning.size());
			if(pos == i->name.npos) continue; /* doesn't contain it */
			if(pos + containing.size() > unsigned(end_pos)) continue; /* found it but it overlaps with the end */
		}

		out.push_back( i->name );
	}
}

void FileSet::GetFilesEqualTo(const CString &str, vector<CString> &out, bool bOnlyDirs) const
{
	set<File>::const_iterator i = files.find( File(str) );
	if(i == files.end())
		return;

	if(bOnlyDirs && !i->dir)
		return;

	out.push_back( i->name );
}

FileType FileSet::GetFileType(const CString &path ) const
{
	set<File>::const_iterator i = files.find( File(path) );
	if(i == files.end())
		return TTYPE_NONE;

	return i->dir? TTYPE_DIR:TTYPE_FILE;
}

int FileSet::GetFileSize(const CString &path) const
{
	set<File>::const_iterator i = files.find( File(path) );
	if(i == files.end())
		return -1;
	return i->size;
}

int FileSet::GetFileHash(const CString &path) const
{
	set<File>::const_iterator i = files.find( File(path) );
	if(i == files.end())
		return -1;
	return i->hash + i->size;
}

/*
 * Given "foo/bar/baz/" or "foo/bar/baz", return "foo/bar/" and "baz".
 * "foo" -> "", "foo"
 */
static void SplitPath( CString Path, CString &Dir, CString &Name )
{
	CollapsePath( Path );
	if( Path.Right(1) == "/" )
		Path.erase( Path.size()-1 );

	unsigned sep = Path.find_last_of( '/' );
	if( sep == CString::npos )
	{
		Dir = "";
		Name = Path;
	}
	else
	{
		Dir = Path.substr( 0, sep+1 );
		Name = Path.substr( sep+1 );
	}
}


FileType FilenameDB::GetFileType( const CString &sPath )
{
	CString Dir, Name;
	SplitPath( sPath, Dir, Name );

	if( Name == "." )
		return TTYPE_DIR;

	FileSet *fs = GetFileSet( Dir );
	return fs->GetFileType( Name );
}


int FilenameDB::GetFileSize( const CString &sPath )
{
	CString Dir, Name;
	SplitPath(sPath, Dir, Name);
	FileSet *fs = GetFileSet( Dir );
	return fs->GetFileSize(Name);
}

int FilenameDB::GetFileHash( const CString &sPath )
{
	CString Dir, Name;
	SplitPath(sPath, Dir, Name);
	FileSet *fs = GetFileSet( Dir );
	return fs->GetFileHash(Name);
}

bool FilenameDB::ResolvePath(CString &path)
{
	if(path == ".") return true;
	if(path == "") return true;

	/* Split path into components. */
	int begin = 0, size = -1;

	/* Resolve each component. */
	CString ret = "";
	FileSet *fs = NULL;
	File *prev_file = NULL;
	static const CString slash("/");
	while( 1 )
	{
		split( path, slash, begin, size, true );
		if( begin == (int) path.size() )
			break;

		if( fs == NULL )
			fs = GetFileSet( ret );

		CString p = path.substr( begin, size );
		set<File>::iterator it = fs->files.find( File(p) );

		/* If there were no matches, the path isn't found. */
		if( it == fs->files.end() )
			return false;

		if( prev_file )
			prev_file->dirp = fs;
		prev_file = (File*) &*it;

		if( ret.size() != 0 )
			ret += "/";
		ret += it->name;

		fs = it->dirp;
	}
	
	if( path.size() && path[path.size()-1] == '/' )
		path = ret + "/";
	else
		path = ret;
	return true;
}

void FilenameDB::GetFilesMatching(const CString &dir, const CString &beginning, const CString &containing, const CString &ending, vector<CString> &out, bool bOnlyDirs)
{
	FileSet *fs = GetFileSet( dir );
	fs->GetFilesMatching(beginning, containing, ending, out, bOnlyDirs);
}

void FilenameDB::GetFilesEqualTo(const CString &dir, const CString &fn, vector<CString> &out, bool bOnlyDirs)
{
	FileSet *fs = GetFileSet( dir );
	fs->GetFilesEqualTo(fn, out, bOnlyDirs);
}


void FilenameDB::GetFilesSimpleMatch(const CString &dir, const CString &fn, vector<CString> &out, bool bOnlyDirs)
{
	/* Does this contain a wildcard? */
	unsigned first_pos = fn.find_first_of('*');
	if(first_pos == fn.npos)
	{
		/* No; just do a regular search. */
		GetFilesEqualTo(dir, fn, out, bOnlyDirs);
	} else {
		unsigned second_pos = fn.find_first_of('*', first_pos+1);
		if(second_pos == fn.npos)
		{
			/* Only one *: "A*B". */
			/* XXX: "_blank.png*.png" shouldn't match the file "_blank.png". */
			GetFilesMatching(dir, fn.substr(0, first_pos), "", fn.substr(first_pos+1), out, bOnlyDirs);
		} else {
			/* Two *s: "A*B*C". */
			GetFilesMatching(dir, 
				fn.substr(0, first_pos),
				fn.substr(first_pos+1, second_pos-first_pos-1),
				fn.substr(second_pos+1), out, bOnlyDirs);
		}
	}
}

FileSet *FilenameDB::GetFileSet( CString dir, bool create )
{
	/* Normalize the path. */
	dir.Replace("\\", "/"); /* foo\bar -> foo/bar */
	dir.Replace("//", "/"); /* foo//bar -> foo/bar */

	if( dir == "" )
		dir = ".";

	CString lower = dir;
	lower.MakeLower();

	map<CString, FileSet *>::iterator i = dirs.find( lower );
	if( !create )
	{
		if( i == dirs.end() )
			return NULL;
		return i->second;
	}

	FileSet *ret;
	if( i != dirs.end() )
	{
		if( ExpireSeconds != -1 && i->second->age.PeekDeltaTime() >= ExpireSeconds )
		{
			ret = i->second;
			ret->age.Touch();
			ret->files.clear();
		} else
			return i->second;
	}
	else
	{
		ret = new FileSet;
		AddFileSet( dir, ret );
	}

	PopulateFileSet( *ret, dir );
	return ret;
}

void FilenameDB::AddFileSet( CString sPath, FileSet *fs )
{
	sPath.MakeLower();
	map<CString, FileSet *>::iterator it = dirs.find( sPath );
	if( it != dirs.end() )
		delete it->second;

	dirs[sPath] = fs;
}

/* Add the file or directory "sPath".  sPath is a directory if it ends with
 * a slash. */
void FilenameDB::AddFile( const CString &sPath, int size, int hash, void *priv )
{
	vector<CString> parts;
	split( sPath, "/", parts, false );

	CStringArray::const_iterator begin = parts.begin();
	CStringArray::const_iterator end = parts.end();

	bool IsDir = true;
	if( sPath[sPath.size()-1] != '/' )
		IsDir = false;
	else
		--end;

	do
	{
		/* Combine all but the last part. */
		CString dir = join( "/", begin, end-1 ) + "/";
		const CString &fn = *(end-1);
		FileSet *fs = GetFileSet( dir );

		File f;
		f.SetName( fn );
		if( fs->files.find( f ) == fs->files.end() )
		{
			f.dir = IsDir;
			if( !IsDir )
			{
				f.size = size;
				f.hash = hash;
				f.priv = priv;
			}
			fs->files.insert( f );
		}
		IsDir = true;

		--end;
	} while( begin != end );
}

void FilenameDB::DelFile( const CString &sPath )
{
	CString lower = sPath;
	lower.MakeLower();

	map<CString, FileSet *>::iterator fsi = dirs.find( lower );
	if( fsi != dirs.end() )
	{
		FileSet *fs = fsi->second;

		/* Remove any stale dirp pointers. */
		for( map<CString, FileSet *>::iterator it = dirs.begin(); it != dirs.end(); ++it )
		{
			FileSet *Clean = it->second;
			for( set<File>::iterator f = Clean->files.begin(); f != Clean->files.end(); ++f )
			{
				File &ff = (File &) *f;
				if( ff.dirp == fs )
					ff.dirp = NULL;
			}
		}

		delete fs;
		dirs.erase( fsi );
	}

	/* Delete sPath from its parent. */
	CString Dir, Name;
	SplitPath(sPath, Dir, Name);
	FileSet *Parent = GetFileSet( Dir, false );
	if( Parent )
	{
		set<File>::iterator i = Parent->files.find( File(Name) );
		if( i != Parent->files.end() )
		{
			Parent->files.erase( i );
		}
	}
}

void FilenameDB::FlushDirCache()
{
	for( map<CString, FileSet *>::iterator i = dirs.begin(); i != dirs.end(); ++i )
		delete i->second;
	dirs.clear();
}

File *FilenameDB::GetFile( const CString &sPath )
{
	CString Dir, Name;
	SplitPath(sPath, Dir, Name);
	FileSet *fs = GetFileSet( Dir );

	set<File>::iterator it;
	it = fs->files.find( File(Name) );
	if( it == fs->files.end() )
		return NULL;

	/* Oops.  &*it is a const File &, because you can't change the order
	 * of something once it's in a list.  Cast away the const; we won't
	 * change the filename (used for the ordering), but the rest of the
	 * values are non-const. */
	return const_cast<File *> (&*it);
}




void FilenameDB::GetDirListing( CString sPath, CStringArray &AddTo, bool bOnlyDirs, bool bReturnPathToo )
{
//	LOG->Trace( "GetDirListing( %s )", sPath.c_str() );

	/* If you want the CWD, use ".". */
	ASSERT(!sPath.empty());

	/* Strip off the last path element and use it as a mask. */
	unsigned pos = sPath.find_last_of( '/' );
	CString fn;
	if( pos == sPath.npos )
	{
		fn = sPath;
		sPath = "";
	} else {
		fn = sPath.substr(pos+1);
		sPath = sPath.substr(0, pos+1);
	}

	/* If the last element was empty, use "*". */
	if( fn.size() == 0 )
		fn = "*";

	unsigned start = AddTo.size();
	GetFilesSimpleMatch(sPath, fn, AddTo, bOnlyDirs);

	if(bReturnPathToo && start < AddTo.size())
	{
		ResolvePath(sPath);
		while(start < AddTo.size())
		{
			AddTo[start] = sPath + AddTo[start];
			start++;
		}
	}
}

bool ResolvePath(CString &path) { return true; } // XXX

