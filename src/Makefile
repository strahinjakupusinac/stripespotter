.PHONY: mac_zip mac_app tester

OS_NAME:=$(shell uname)
VPATH:=core db exif gui utils
BUILDDIR:=build
CXXFLAGS:=-g $(addprefix -I,$(VPATH)) $(shell wx-config --cxxflags)
LDFLAGS:=-g $(shell wx-config --libs)

ifeq ($(OS_NAME),Darwin)
	CXXFLAGS:=$(CXXFLAGS) -arch i386
	LDFLAGS:=$(CXXFLAGS) -arch i386
endif

APP:=StripeSpotter
VERSION:=2.0
app_obj = FloatImage.o MultiScaleHistogram.o StripeCode.o wxImageUtils.o \
	db-csv.o exif.o AddPicturesDialog.o GUIFrame.o ImageFrame.o \
	SavePictureDialog.o StripeSpotterApp.o StripeSpotterMain.o

TESTER:=benchmarker
tester_obj = benchmarker.o StripeCode.o wxImageUtils.o mt19937ar.o \
	 FloatImage.o utils.o MultiScaleHistogram.o

# first rule = default rule
all: $(BUILDDIR)/$(APP)

tester: $(BUILDDIR)/$(TESTER)

mac_zip: $(APP)-v$(VERSION)-mac.zip
mac_app: $(APP).app

$(APP)-v%.zip: $(BUILDDIR)/$(APP) $(APP).app Info.plist AppIcon.icns
	zip -r $@ $(APP).app

$(APP).app: $(BUILDDIR)/$(APP) Info.plist AppIcon.icns
	mkdir -p $@/Contents/MacOS
	mkdir -p $@/Contents/Resources
	cp gui/icon-zebra.bmp $@/Contents/Resources
	cp Info.plist $@/Contents
	cp $(BUILDDIR)/$(APP) $@/Contents/MacOS/$(APP)
	cp AppIcon.icns $@/Contents/Resources

$(BUILDDIR)/$(APP): $(addprefix $(BUILDDIR)/,$(app_obj))
	$(CXX) $(LDFLAGS) $^ -o $@

$(BUILDDIR)/$(TESTER): $(addprefix $(BUILDDIR)/,$(tester_obj))
	$(CXX) $(LDFLAGS) $^ -o $@

$(BUILDDIR)/%.o: %.cpp | $(BUILDDIR)
	$(CXX) -c $(CXXFLAGS) $< -o $@

$(BUILDDIR):
	mkdir -p $@

clean:
	rm -rf $(APP).app $(BUILDDIR)
