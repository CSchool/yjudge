package main

import (
	"context"
	"crypto/sha256"
	"flag"
	"fmt"
	"io"
	"log"
	"net/http"
	"os"
	"os/signal"
	"path"
	"regexp"
)

var storePath string
var hashRegexp *regexp.Regexp = regexp.MustCompile("^/[0-9a-f]{64}")

type server struct{}

func getPathForHash(hash []byte) string {
	return path.Join(storePath, string(hash[:2]), string(hash[2:4]), string(hash))
}

func (s *server) ServeHTTP(w http.ResponseWriter, r *http.Request) {
	if r.Method == "GET" {
		if !hashRegexp.MatchString(r.URL.String()) {
			w.WriteHeader(http.StatusBadRequest)
			return
		}

		hash := []byte(r.URL.String())[1:]
		http.ServeFile(w, r, getPathForHash(hash))
	} else if r.Method == "POST" {
		file, _, err := r.FormFile("file")
		if err != nil {
			w.WriteHeader(http.StatusBadRequest)
			return
		}
		defer file.Close()

		hasher := sha256.New()
		if _, err := io.Copy(hasher, file); err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			log.Printf("Hashing file: %s", err)
			return
		}

		if _, err := file.Seek(0, 0); err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			log.Printf("Seek failed: %s", err)
			return
		}

		hash := fmt.Sprintf("%x", hasher.Sum(nil))

		filePath := getPathForHash([]byte(hash))
		if err := os.MkdirAll(path.Dir(filePath), os.ModePerm); err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			log.Printf("Creating directories failed: %s", err)
			return
		}

		f, err := os.Create(filePath)
		if err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			log.Printf("Opening file for writing failed: %s", err)
			return
		}
		defer f.Close()

		if _, err := io.Copy(f, file); err != nil {
			w.WriteHeader(http.StatusInternalServerError)
			log.Printf("Writing file: %s", err)
			return
		}

		fmt.Fprintf(w, hash)
	} else if r.Method == "DELETE" {
		// TODO
	} else {
		w.WriteHeader(http.StatusMethodNotAllowed)
		return
	}
}

func main() {
	addr := flag.String("addr", "0.0.0.0:5000", "bind address")
	store := flag.String("store", "store", "storage directory")

	flag.Parse()

	storePath = *store

	log.Printf("Serving files from '%s'", storePath)

	stop := make(chan os.Signal, 1)
	signal.Notify(stop, os.Interrupt)

	h := &http.Server{Addr: *addr, Handler: &server{}}

	go func() {
		if err := h.ListenAndServe(); err != nil {
			log.Fatal(err)
		}
	}()

	<-stop

	if err := h.Shutdown(context.Background()); err != nil {
		log.Fatal(err)
	}
}
