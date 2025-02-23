package lib

import (
	"bytes"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/exec"
	"sort"
	"strconv"
	"strings"
	"time"
)

type Quint [5]byte // Quintabyte commands.

func NewQuint(cmd byte, n uint, p uint) Quint {
	var q Quint
	q[0] = cmd
	q[1], q[2] = Hi(n), Lo(n)
	q[3], q[4] = Hi(p), Lo(p)
	return q
}

func (q Quint) Command() byte {
	return q[0]
}
func (q Quint) N() uint {
	return HiLo(q[1], q[2])
}
func (q Quint) P() uint {
	return HiLo(q[3], q[4])
}

/* TODO

func ReadQuint(conn net.Conn) []bytes {
  var q Quint
  _, err := io.ReadFull(conn, q[:])
  Check(err)
  n := q.N()
  bb := make([]byte, n)
  _, err = io.ReadFull(conn, bb)
  Check(err)
}

func WriteQuint(conn net.Conn, cmd byte, p uint, bb []bytes) {
  n := len(bytes)
  q := NewQuint(cmd, uint(n), p)
  _, err := conn.Write(q[:])
  Check(err)
  _, err := conn.Write(bb)
  Check(err)
}

TODO */

func (ses *Session) ReplyOnChannel(cmd byte, p uint, pay []byte) {
	n := uint(len(pay))
	log.Printf("PAY OUT [%d.] %v", n, pay)
	log.Printf("        [$%x] %q", n, pay)
	buf := make([]byte, 5+n)
	buf[0] = cmd
	buf[1], buf[2] = Hi(n), Lo(n)
	buf[3], buf[4] = Hi(p), Lo(p)
	copy(buf[5:], pay)

	log.Printf("SENDING %02x", buf)
	cc, err := ses.Conn.Write(buf)
	if err != nil {
		log.Panicf("SendQandP(%q) got err: %v", ses.ID, err)
	}
	if uint(cc) != 5+n {
		log.Panicf("SendQandP(%q) short write: %v", ses.ID, cc)
	}
}

type Screen interface {
	Clear()
	PutChar(ch byte)
	PutStr(s string)
	//Width() int
	//Height() int
	//Push(*Session)
	//Redraw()
}

type AxScreen struct {
	Ses *Session
}

func (ax *AxScreen) Clear() {
	for i := 0; i < 16; i++ {
		ax.PutChar(13)
	}
}
func (ax *AxScreen) PutChar(ch byte) {
	q := NewQuint(CMD_PUTCHAR, 0, uint(ch))
	_, err := ax.Ses.Conn.Write(q[:])
	Check(err)
}
func (ax *AxScreen) PutStr(s string) {
	for _, r := range s {
		AssertLT(r, 127)
		ax.PutChar(byte(r))
	}
}

/*
# type TextScreen struct {
# 	Old  []byte // screen RAM already sent.
# 	New  []byte // screen RAM not yet sent.
# 	W, H int    // screen dimensions
# 	P    int    // cursor
# 	Addr int
# }
#
# func NewTextScreen(w, h int, addr int) *TextScreen {
# 	o := make([]byte, w*h)
# 	n := make([]byte, w*h)
# 	for i := 0; i < w*h; i++ {
# 		o[i] = ' '
# 		n[i] = ' '
# 	}
#
# 	return &TextScreen{
# 		Old:  o,
# 		New:  n,
# 		W:    w,
# 		H:    h,
# 		P:    0,
# 		Addr: addr,
# 	}
# }
#
# func (t *TextScreen) Clear() {
# 	t.New = make([]byte, t.W*t.H)
# 	for i := range t.New {
# 		t.New[i] = 32
# 	}
# }
# func (t *TextScreen) PutChar(ch byte) {
# 	if ch == 10 || ch == 13 {
# 		PutChar(' ')
# 		for (t.P & (t.W - 1)) != 0 {
# 			PutChar(' ')
# 		}
# 		return
# 	}
#
# 	if ' ' <= ch && ch <= '~' {
# 		t.New[t.P] = ch
# 		t.P++
# 		for t.P >= t.W*t.H {
# 			for i := t.W; i < t.W*t.H; i++ {
# 				t.New[i-t.W] = t.New[i]
# 			}
# 		}
# 	}
# }
*/

type LineBuf struct {
	Ses *Session
}

func (lb *LineBuf) GetLine() string {
	var buf []byte
	for {
		var q Quint
		_, err := io.ReadFull(lb.Ses.Conn, q[:])
		Check(err)
		cmd := q.Command()

		switch cmd {
		case CMD_INKEY:
			ch := byte(q.P())
			switch ch {
			case 10, 13:
				lb.Ses.Screen.PutChar(ch)
				return string(buf)
			case 8:
				if len(buf) > 0 {
					buf = buf[:len(buf)-1] // trim last
					lb.Ses.Screen.PutChar(ch)
				}
			default:
				if ' ' <= ch && ch <= '~' {
					buf = append(buf, ch)
					lb.Ses.Screen.PutChar(ch)
				} else {
					// log.Panicf("LineBuf: weird char: %d", ch)
				}
			} // switch ch
		default:
			log.Panicf("bad packet to GetLine: cmd=%d", cmd)
		} // switch cmd
	}
}

type Session struct {
	ID      string
	Conn    net.Conn
	Screen  Screen
	LineBuf *LineBuf

	Block0 *os.File
	Block1 *os.File

	Procs map[uint]*Proc

	// Env     map[string]string // for whatever
	// User    *User             // logged in user
	// Card *Card // Current card.
}

func (ses *Session) String() string {
	return ses.ID
}

// Run() is called with "go" to manage a connection session.
var nextID int = 1001

func NewSession(conn net.Conn) *Session {
	ses := &Session{
		ID:    fmt.Sprintf("ses%d", nextID),
		Conn:  conn,
		Procs: make(map[uint]*Proc),
	}
	ses.Screen = &AxScreen{ses}
	ses.LineBuf = &LineBuf{ses}
	nextID++

	return ses
}

var Cards = make(map[int]*Card)

type Card struct {
	Num  int
	Name string
	Moms []int
	Kids map[int]*Card

	Text string

	Launch string
	Block0 string
	Block1 string
}

func Add(num int, parent int, name string, tc *Card) {
	tc.Num = num
	tc.Name = name
	tc.Moms = append(tc.Moms, parent)
	tc.Kids = make(map[int]*Card)

	Cards[num] = tc
	if num > 0 {
		for _, mom := range tc.Moms {
			parent, ok := Cards[mom]
			if !ok {
				log.Panicf("Add Card #%d=%q: cannot find mom %d", num, name, mom)
			}
			parent.Kids[num] = tc
		}
	}
}

func DuplicateFileToTemp(filename string, startup string) *os.File {
	r, err := os.Open(filename)
	Check(err)
	tmp, err := os.CreateTemp("/tmp", "tmp.waiter.*.tmp")
	Check(err)
	_, err = io.Copy(tmp, r)
	Check(err)

	err = r.Close()
	Check(err)

	log.Printf("Duplicated %q => %q", filename, tmp.Name())
	if startup != "" {
		shell := Format("set -x; echo '%s' | os9 copy -l -r /dev/stdin '%s,startup' ", startup, tmp.Name())
		cmd := exec.Command("/bin/sh", "-c", shell)
		err := cmd.Run()
		if err != nil {
			log.Panicf("Installing startup failed: %q => %v", shell, err)
		}
	}

	err = os.Remove(tmp.Name())
	Check(err)
	return tmp
}

func NonduplicatingDuplicateFileToTemp(filename string) *os.File {
	r, err := os.OpenFile(filename, os.O_RDWR, 0777)
	Check(err)
	return r
}

func Bold(s string) string {
	var bb bytes.Buffer
	for _, r := range s {
		bb.WriteByte(byte(r))
		bb.WriteByte(1)
	}
	return bb.String()
}

func BoldInt(x int) string {
	return Bold(fmt.Sprintf("%d", x))
}

type NumStr struct {
	Num int
	Str string
}
type NumStrSlice []NumStr

func (o NumStrSlice) Len() int           { return len(o) }
func (o NumStrSlice) Less(i, j int) bool { return o[i].Num < o[j].Num }
func (o NumStrSlice) Swap(i, j int)      { o[i], o[j] = o[j], o[i] }

func Run(ses *Session) {
	defer func() {
		r := recover()
		if r != nil {
			ses.Screen.PutStr(fmt.Sprintf("\n\n(session Run) ERROR: %v\n", r))
			panic(r)
		}
	}()

	current := Cards[0]

CARD:
	for {
		log.Printf("== %d == %q ==", current.Num, current.Name)
		log.Printf("%#v", *current)
		ses.Screen.Clear()
		ses.Screen.PutStr(fmt.Sprintf("== %s == %s ==\n", BoldInt(current.Num), Bold(current.Name)))
		ses.Screen.PutStr(current.Text + "\n")

		if current.Launch != "" {
			ses.Screen.PutStr("[@] Launch!\n")
		}

		// Sort kids and show them.
		var vec NumStrSlice
		for num, kid := range current.Kids {
			vec = append(vec, NumStr{num, fmt.Sprintf("[%d] %s\n", num, kid.Name)})
		}
		sort.Sort(vec)
		for _, ns := range vec {
			ses.Screen.PutStr(ns.Str)
		}
		ses.Screen.PutStr(">\001")

		line := ses.LineBuf.GetLine()
		line = strings.Trim(line, " \t\r\n")
		log.Printf("GetLine => %q", line)
		aNum, err := strconv.Atoi(line)
		if err == nil {
			// It was a number.
			card, ok := Cards[aNum]
			if ok {
				current = card
			} else {
				ses.Screen.PutStr(fmt.Sprintf("\nError: Not a number: %q", line))
				goto DELAY
			}
		} else if line == "@" {

			if current.Launch == "" {
				ses.Screen.PutStr("There is no \"@\" command on this page.")
				goto DELAY
			}

			if current.Block0 != "" {
				tail := strings.TrimPrefix(current.Block0, ".")
				log.Printf("Block0: %q", tail)
				ses.Screen.PutStr(fmt.Sprintf("Block0: %q", tail))
				ses.Block0 = DuplicateFileToTemp(*READONLY+"/"+tail, "")
			}

			if strings.HasPrefix(current.Launch, "@") {
				name := current.Launch[1:]
				demo, ok := Demos[name]
				if !ok {
					for k := range Demos {
						log.Printf("known demo: %q", k)
					}
					log.Printf("Unknown demo: %q", name)
					ses.Screen.PutStr(fmt.Sprintf("\nUnknown Demo: %q", name))
					goto DELAY
				}
				demo(ses.Conn)
				return
			}

			tail := strings.TrimPrefix(current.Launch, ".")
			log.Printf("Upload: %q", tail)
			UploadProgram(ses.Conn, *READONLY+"/"+tail)
			ReadFiveLoop(ses.Conn, ses)
			return
		} else {
			// It was something else.
			ses.Screen.PutStr(fmt.Sprintf("\nError: Not a number: %q", line))
			goto DELAY
		}
		continue CARD
	DELAY:
		time.Sleep(3 * time.Second)
	}
}
