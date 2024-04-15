package lib

import (
	"bytes"
	"flag"
	. "github.com/strickyak/frobio/frob3/lemma/util"
	"io/ioutil"
	"log"
	"os"
	PFP "path/filepath"
	"strings"
	"time"
)

var FlagHomesRoot = flag.String("homes_root", "", "User home directories, for HdbDos")
var FlagPublicRoot = flag.String("public_root", "", "public files, for HdbDos")
var FlagSideloadRaw = flag.String("sideload_raw", "sideload.lwraw", "raw machine code fragment for HdbDos")
var FlagInkeyRaw = flag.String("inkey_raw", "inkey_trap.lwraw", "raw machine code fragment for HdbDos")

const NumDrives = 10
const DirPerm = 0775
const FilePerm = 0664

const FloppySize = 161280

type HdbDosDrive struct {
	Path  string
	Image []byte
	Dirty bool
}

type HdbDosSession struct {
	NumReads  int64
	SavedText []byte

	Drives [NumDrives]*HdbDosDrive
}

func (h *HdbDosSession) SetDrive(drive byte, c *Chooser) {
	log.Printf("SetDrive: %d %v", drive, *c)

	// Create the drive record, if needed.  Call it d.
	if h.Drives[drive] == nil {
		h.Drives[drive] = &HdbDosDrive{}
		log.Printf("@@@@@@@@@@ created HdbDosDrive{} number %d.", drive)
	}
	d := h.Drives[drive]

	if !c.IsDir && c.Size == FloppySize {
		// TODO: if drive is dirty, save it somewhere.
		d.Path = c.Path()
		d.Image = nil
		d.Dirty = false
	}
}

func TextChooserShell(ses *Session) {
	t := &TextVGA{
		Ses: ses,
	}
	t.Loop() // Loop until we return from Hijack.
}

func CreateHomeIfNeeded(ses *Session) {
	os.Mkdir(*FlagHomesRoot, 0770)
	os.Mkdir(PFP.Join(*FlagHomesRoot, ses.Hostname), DirPerm)
}

// Entry from waiter.go for a Hijack.
func HdbDosHijack(ses *Session, payload []byte) {
	savedText := payload // default text screen: $0400 to $05FF

	const GIME = 0xFF90
	const GIME_LEN = 0x0050

	savedGime := Peek2Ram(ses.Conn, GIME, GIME_LEN)
	DumpHexLines("savedGime", GIME, savedGime)

	CreateHomeIfNeeded(ses)
	TextChooserShell(ses)

	// Restore screen
	PokeRam(ses.Conn, 0x0400, savedText)
	//PokeRam(ses.Conn, GIME, savedGime)
	// We tell coco that this is the END of the HIJACK.
	WriteQuint(ses.Conn, CMD_HDBDOS_HIJACK, 0, []byte{})
}

func HdbDosCleanupOneDrive(ses *Session, driveNum int, timestamp string) {
	drive := ses.HdbDos.Drives[driveNum]
	if drive == nil || !drive.Dirty {
		return
	}
	// if dirty:
	dest := PFP.Join(*FlagHomesRoot, ses.Hostname, PFP.Base(drive.Path)) + "-" + timestamp
	err := os.WriteFile(dest, drive.Image, FilePerm)
	if err != nil {
		log.Printf("BAD: HdbDosCleanup: Error saving dirty file %d bytes %q as %q: %v", len(drive.Image), drive.Path, dest, err)
	} else {
		log.Printf("HdbDosCleanup: Saved dirty file %d bytes %q as %q", len(drive.Image), drive.Path, dest)
	}
	ses.HdbDos.Drives[driveNum] = nil // Delete the drive record.
}
func HdbDosCleanup(ses *Session) {
	timestamp := time.Now().Format("2006-01-02-150405")
	for driveNum, _ := range ses.HdbDos.Drives {
		HdbDosCleanupOneDrive(ses, driveNum, timestamp)
	}
}

// Entry from waiter.go for a Sector.
func HdbDosSector(ses *Session, payload []byte) {
	if ses.HdbDos == nil {
		ses.HdbDos = &HdbDosSession{}
	}
	ses.Cleanups = append(ses.Cleanups, func() {
		HdbDosCleanup(ses)
	})

	h := ses.HdbDos
	cmd := payload[0]
	lsn3 := uint(payload[1])
	lsn2 := uint(payload[2])
	lsn1 := uint(payload[3])
	lsn0 := uint(payload[4])
	lsnBig := (lsn3 << 24) | (lsn2 << 16) | (lsn1 << 8) | lsn0

	drive := lsnBig / 630
	lsn := lsnBig % 630
	front := 256 * lsn
	back := front + 256

	log.Printf("HdbDos cmd=%x drive=%x lsn=%x paylen=%d.", cmd, drive, lsn, len(payload))
	DumpHexLines("payload", 0, payload)

	// Create the drive record, if needed.  Call it d.
	if h.Drives[drive] == nil {
		h.Drives[drive] = &HdbDosDrive{}
		log.Printf("@@@@@@@@@@ created HdbDosDrive{} number %d.", drive)
	}
	d := h.Drives[drive]

	// Open the drive, if needed.
	// Has it been mounted?
	if d.Path == "" {
		// No mount, so make it an empty disk.
		d.Path = "--new--" // temporary bogus name
		d.Image = EmptyDecbInitializedDiskImage()
		d.Dirty = false
		log.Printf("@@@@@@@@@@ made empty HdbDosDrive{} number %d.", drive)
	}
	if d.Image == nil {
		bb, err := os.ReadFile(PFP.Join(*FlagPublicRoot, d.Path))
		if err != nil {
			log.Panicf("HdbDosSector ReadFile failed %q: %v", d.Path, err)
		}
		if len(bb) != 161280 {
			log.Panicf("HdbDosSector ReadFile got %d bytes, wanted 161280", len(bb))
		}
		d.Image = bb
		d.Dirty = false
		log.Printf("@@@@@@@@@@ opend file %q HdbDosDrive{} number %d.", d.Path, drive)
	}

	switch cmd {
	case 2: // Read
		h.NumReads++
		if true && h.NumReads == 1 {
			SendInitialInjections(ses)
		}

		buf := make([]byte, 256+5)
		for i := 0; i < 5; i++ {
			buf[i] = payload[i]
		}
		copy(buf[5:], d.Image[front:back])
		log.Printf("HDB READ lsn=%d.: %02x", lsn, buf[5:])

		log.Printf("HdbDos Reply Packet: quint + %d.", len(buf))
		DumpHexLines("REPLY", 0, buf)
		WriteQuint(ses.Conn, CMD_HDBDOS_SECTOR, 0, buf)

	case 3: // Write
		copy(d.Image[front:back], payload[5:])
		d.Dirty = true
		log.Printf("HDB WRITE lsn=%d.: %02x", lsn, payload[5:])

	default:
		panic(cmd)
	}
}

func Inject(ses *Session, sideload []byte, dest uint, exec bool, payload []byte) {
	log.Printf("Inject: len=%4x dest=%4x exec=%v", len(payload), dest, exec)
	buf := make([]byte, 5+256)
	copy(buf[5:], sideload)
	copy(buf[5+64:], payload)

	buf[5+0x3C] = byte(len(payload))
	buf[5+0x38] = byte(dest >> 8)
	buf[5+0x39] = byte(dest)

	if exec {
		buf[5+0x3A] = byte(dest >> 8)
		buf[5+0x3B] = byte(dest)
	}

	WriteQuint(ses.Conn, CMD_HDBDOS_EXEC, 0, buf)
	DumpHexLines("Inject: did ", dest, buf)
}

func AsciiToInjectBytes(s string) []byte {
	var bb bytes.Buffer
	for _, ch := range strings.ToUpper(s) {
		bb.WriteByte(63 & byte(ch))
	}
	return bb.Bytes()
}

func SendInitialInjections(ses *Session) {
	//var splash []byte
	//for i := 0; i < 32; i++ {
	//// Splash some semigraphics chars
	//splash = append(splash, byte(256-32+i))
	//}
	/*
		const splashStr = "CLEAR KEY TOGGLES DISK CHOOSER"
		var bb bytes.Buffer
		for _, ch := range splashStr {
			bb.WriteByte(63 & byte(ch))
		}
		splash := bb.Bytes()
	*/

	sideload := Value(ioutil.ReadFile(*FlagSideloadRaw))
	Inject(ses, sideload, 0x0580 /* on text screen*/, false, AsciiToInjectBytes("CLEAR KEY TOGGLES DISK CHOOSER:"))
	Inject(ses, sideload, 0x05A0 /* on text screen*/, false, AsciiToInjectBytes("  USE ARROW KEYS TO NAVIGATE."))
	Inject(ses, sideload, 0x05C0 /* on text screen*/, false, AsciiToInjectBytes("  HIT 0 TO MOUNT DRIVE 0,"))
	Inject(ses, sideload, 0x05E0 /* on text screen*/, false, AsciiToInjectBytes("  1 FOR 1, ... THROUGH 9."))

	bb := Value(ioutil.ReadFile(*FlagInkeyRaw))

	const InkeyTrapInit = 0xFA12
	const ChunkSize = 0xC0
	endAddr := uint(InkeyTrapInit + len(bb))
	for len(bb) > 0 {
		n := len(bb)
		if n > ChunkSize {
			// All-but-last injection:
			endAddr -= ChunkSize
			Inject(ses, sideload, endAddr, false, bb[n-ChunkSize:])
			bb = bb[:n-ChunkSize]
		} else {
			// Last injection:
			Inject(ses, sideload, InkeyTrapInit, true, bb)
			bb = bb[:0]
		}
	}
}

func EmptyDecbInitializedDiskImage() []byte {
	/*
		$ decb dskini /tmp/empty
		$ hd /tmp/empty
		00000000  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff
		*
		00013200  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
		*
		00013300  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff
		*
		00013340  ff ff ff ff 00 00 00 00  00 00 00 00 00 00 00 00
		00013350  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00
		*
		00013400  ff ff ff ff ff ff ff ff  ff ff ff ff ff ff ff ff
		*
		00027600
		$
	*/

	bb := make([]byte, 161280)
	for i := 0; i < 0x00013200; i++ {
		bb[i] = 0xFF
	}
	for i := 0x00013300; i < 0x00013344; i++ {
		bb[i] = 0xFF
	}
	for i := 0x00013400; i < 0x00027600; i++ {
		bb[i] = 0xFF
	}
	return bb
}
