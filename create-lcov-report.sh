# Creating report
set -ex
if [ x$TRAVIS_BUILD_DIR != x ]; then
  cd $TRAVIS_BUILD_DIR
fi
echo $PWD
lcov --directory . --capture --output-file coverage.info # capture coverage info
lcov --remove coverage.info '/usr/*' '*/usr/*' '*/3rd-party/*' --output-file coverage.info # filter out system and 3rd-party stuff
lcov --list coverage.info #debug info