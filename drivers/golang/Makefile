GRAMMAR_DIR= $(shell pwd)/parser
ANTLR_ARTIFACTS = $(GRAMMAR_DIR)/age_base_listener.go $(GRAMMAR_DIR)/age_base_visitor.go $(GRAMMAR_DIR)/age_lexer.go $(GRAMMAR_DIR)/age_listener.go $(GRAMMAR_DIR)/age_visitor.go $(GRAMMAR_DIR)/age_parser.go
SAMPLES_BIN = samples.bin
ANTLR_VERSION=4.13.0
ANTLR_TARGET=antlr-$(ANTLR_VERSION)-complete.jar

all: build test

$(ANTLR_TARGET):
	curl https://www.antlr.org/download/$(ANTLR_TARGET)> $(ANTLR_TARGET)

$(ANTLR_ARTIFACTS): $(ANTLR_TARGET) $(GRAMMAR_DIR)/Age.g4
	java -Xmx500M -cp $(ANTLR_TARGET) org.antlr.v4.Tool -Dlanguage=Go -visitor $(GRAMMAR_DIR)/Age.g4

generate: $(ANTLR_ARTIFACTS)

test: $(ANTLR_ARTIFACTS)
	go test -v ./...

build: $(ANTLR_ARTIFACTS)
	go build -o $(SAMPLES_BIN) ./samples

clean:
	rm $(ANTLR_ARTIFACTS)
	rm $(SAMPLES_BIN)