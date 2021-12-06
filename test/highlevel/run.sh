# Get root of git repository
reporoot=`git rev-parse --show-toplevel`
cd $reporoot

# Create environment if it doesn't exist yet
powerloader=`micromamba env list | grep "powerloader"`
if [[ $powerloader != *"powerloader"* ]]; then
    echo "powerloader not found => creating it"
    micromamba create -f environment.yml
    micromamba activate powerloader
    # micromamba install -c conda-forge zchunk
    # Todo: is there a better way to do this?
fi

micromamba activate powerloader

# 
mkdir $reporoot/build > /dev/null 2>&1         # Ignore errors - todo: is there a better way?
cd $reporoot/build
rm CMakeCache.txt > /dev/null 2>&1   # Ignore errors - todo: better way?
cmake -G Ninja $reporoot
ninja

pytest $reporoot/test/highlevel/run_tests.py

