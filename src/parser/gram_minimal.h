/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

#ifndef YY_MINIMAL_BASE_YY_GRAM_MINIMAL_H_INCLUDED
# define YY_MINIMAL_BASE_YY_GRAM_MINIMAL_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 0
#endif
#if YYDEBUG
extern int minimal_base_yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    IDENT = 258,
    UIDENT = 259,
    FCONST = 260,
    SCONST = 261,
    USCONST = 262,
    BCONST = 263,
    XCONST = 264,
    Op = 265,
    ICONST = 266,
    PARAM = 267,
    TYPECAST = 268,
    DOT_DOT = 269,
    COLON_EQUALS = 270,
    EQUALS_GREATER = 271,
    LESS_EQUALS = 272,
    GREATER_EQUALS = 273,
    NOT_EQUALS = 274,
    ABORT_P = 275,
    ABSOLUTE_P = 276,
    ACCESS = 277,
    ACTION = 278,
    ADD_P = 279,
    ADMIN = 280,
    AFTER = 281,
    AGGREGATE = 282,
    ALL = 283,
    ALSO = 284,
    ALTER = 285,
    ALWAYS = 286,
    ANALYSE = 287,
    ANALYZE = 288,
    AND = 289,
    ANY = 290,
    ARRAY = 291,
    AS = 292,
    ASC = 293,
    ASENSITIVE = 294,
    ASSERTION = 295,
    ASSIGNMENT = 296,
    ASYMMETRIC = 297,
    ATOMIC = 298,
    AT = 299,
    ATTACH = 300,
    ATTRIBUTE = 301,
    AUTHORIZATION = 302,
    BACKWARD = 303,
    BEFORE = 304,
    BEGIN_P = 305,
    BETWEEN = 306,
    BIGINT = 307,
    BINARY = 308,
    BIT = 309,
    BOOLEAN_P = 310,
    BOTH = 311,
    BREADTH = 312,
    BY = 313,
    CACHE = 314,
    CALL = 315,
    CALLED = 316,
    CASCADE = 317,
    CASCADED = 318,
    CASE = 319,
    CAST = 320,
    CATALOG_P = 321,
    CHAIN = 322,
    CHAR_P = 323,
    CHARACTER = 324,
    CHARACTERISTICS = 325,
    CHECK = 326,
    CHECKPOINT = 327,
    CLASS = 328,
    CLOSE = 329,
    CLUSTER = 330,
    COALESCE = 331,
    COLLATE = 332,
    COLLATION = 333,
    COLUMN = 334,
    COLUMNS = 335,
    COMMENT = 336,
    COMMENTS = 337,
    COMMIT = 338,
    COMMITTED = 339,
    COMPRESSION = 340,
    CONCURRENTLY = 341,
    CONFIGURATION = 342,
    CONFLICT = 343,
    CONNECTION = 344,
    CONSTRAINT = 345,
    CONSTRAINTS = 346,
    CONTENT_P = 347,
    CONTINUE_P = 348,
    CONVERSION_P = 349,
    COPY = 350,
    COST = 351,
    CREATE = 352,
    CROSS = 353,
    CSV = 354,
    CUBE = 355,
    CURRENT_P = 356,
    CURRENT_CATALOG = 357,
    CURRENT_DATE = 358,
    CURRENT_ROLE = 359,
    CURRENT_SCHEMA = 360,
    CURRENT_TIME = 361,
    CURRENT_TIMESTAMP = 362,
    CURRENT_USER = 363,
    CURSOR = 364,
    CYCLE = 365,
    DATA_P = 366,
    DATABASE = 367,
    DAY_P = 368,
    DEALLOCATE = 369,
    DEC = 370,
    DECIMAL_P = 371,
    DECLARE = 372,
    DEFAULT = 373,
    DEFAULTS = 374,
    DEFERRABLE = 375,
    DEFERRED = 376,
    DEFINER = 377,
    DELETE_P = 378,
    DELIMITER = 379,
    DELIMITERS = 380,
    DEPENDS = 381,
    DEPTH = 382,
    DESC = 383,
    DETACH = 384,
    DICTIONARY = 385,
    DISABLE_P = 386,
    DISCARD = 387,
    DISTINCT = 388,
    DO = 389,
    DOCUMENT_P = 390,
    DOMAIN_P = 391,
    DOUBLE_P = 392,
    DROP = 393,
    EACH = 394,
    ELSE = 395,
    ENABLE_P = 396,
    ENCODING = 397,
    ENCRYPTED = 398,
    END_P = 399,
    ENUM_P = 400,
    ESCAPE = 401,
    EVENT = 402,
    EXCEPT = 403,
    EXCLUDE = 404,
    EXCLUDING = 405,
    EXCLUSIVE = 406,
    EXECUTE = 407,
    EXISTS = 408,
    EXPLAIN = 409,
    EXPRESSION = 410,
    EXTENSION = 411,
    EXTERNAL = 412,
    EXTRACT = 413,
    FALSE_P = 414,
    FAMILY = 415,
    FETCH = 416,
    FILTER = 417,
    FINALIZE = 418,
    FIRST_P = 419,
    FLOAT_P = 420,
    FOLLOWING = 421,
    FOR = 422,
    FORCE = 423,
    FOREIGN = 424,
    FORWARD = 425,
    FREEZE = 426,
    FROM = 427,
    FULL = 428,
    FUNCTION = 429,
    FUNCTIONS = 430,
    GENERATED = 431,
    GLOBAL = 432,
    GRANT = 433,
    GRANTED = 434,
    GREATEST = 435,
    GROUP_P = 436,
    GROUPING = 437,
    GROUPS = 438,
    HANDLER = 439,
    HAVING = 440,
    HEADER_P = 441,
    HOLD = 442,
    HOUR_P = 443,
    IDENTITY_P = 444,
    IF_P = 445,
    ILIKE = 446,
    IMMEDIATE = 447,
    IMMUTABLE = 448,
    IMPLICIT_P = 449,
    IMPORT_P = 450,
    IN_P = 451,
    INCLUDE = 452,
    INCLUDING = 453,
    INCREMENT = 454,
    INDEX = 455,
    INDEXES = 456,
    INHERIT = 457,
    INHERITS = 458,
    INITIALLY = 459,
    INLINE_P = 460,
    INNER_P = 461,
    INOUT = 462,
    INPUT_P = 463,
    INSENSITIVE = 464,
    INSERT = 465,
    INSTEAD = 466,
    INT_P = 467,
    INTEGER = 468,
    INTERSECT = 469,
    INTERVAL = 470,
    INTO = 471,
    INVOKER = 472,
    IS = 473,
    ISNULL = 474,
    ISOLATION = 475,
    JOIN = 476,
    KEY = 477,
    LABEL = 478,
    LANGUAGE = 479,
    LARGE_P = 480,
    LAST_P = 481,
    LATERAL_P = 482,
    LEADING = 483,
    LEAKPROOF = 484,
    LEAST = 485,
    LEFT = 486,
    LEVEL = 487,
    LIKE = 488,
    LIMIT = 489,
    LISTEN = 490,
    LOAD = 491,
    LOCAL = 492,
    LOCALTIME = 493,
    LOCALTIMESTAMP = 494,
    LOCATION = 495,
    LOCK_P = 496,
    LOCKED = 497,
    LOGGED = 498,
    MAPPING = 499,
    MATCH = 500,
    MATCHED = 501,
    MATERIALIZED = 502,
    MAXVALUE = 503,
    MERGE = 504,
    METHOD = 505,
    MINUTE_P = 506,
    MINVALUE = 507,
    MODE = 508,
    MONTH_P = 509,
    MOVE = 510,
    NAME_P = 511,
    NAMES = 512,
    NATIONAL = 513,
    NATURAL = 514,
    NCHAR = 515,
    NEW = 516,
    NEXT = 517,
    NFC = 518,
    NFD = 519,
    NFKC = 520,
    NFKD = 521,
    NO = 522,
    NONE = 523,
    NORMALIZE = 524,
    NORMALIZED = 525,
    NOT = 526,
    NOTHING = 527,
    NOTIFY = 528,
    NOTNULL = 529,
    NOWAIT = 530,
    NULL_P = 531,
    NULLIF = 532,
    NULLS_P = 533,
    NUMERIC = 534,
    OBJECT_P = 535,
    OF = 536,
    OFF = 537,
    OFFSET = 538,
    OIDS = 539,
    OLD = 540,
    ON = 541,
    ONLY = 542,
    OPERATOR = 543,
    OPTION = 544,
    OPTIONS = 545,
    OR = 546,
    ORDER = 547,
    ORDINALITY = 548,
    OTHERS = 549,
    OUT_P = 550,
    OUTER_P = 551,
    OVER = 552,
    OVERLAPS = 553,
    OVERLAY = 554,
    OVERRIDING = 555,
    OWNED = 556,
    OWNER = 557,
    PARALLEL = 558,
    PARAMETER = 559,
    PARSER = 560,
    PARTIAL = 561,
    PARTITION = 562,
    PASSING = 563,
    PASSWORD = 564,
    PGPOOL = 565,
    PLACING = 566,
    PLANS = 567,
    POLICY = 568,
    POSITION = 569,
    PRECEDING = 570,
    PRECISION = 571,
    PRESERVE = 572,
    PREPARE = 573,
    PREPARED = 574,
    PRIMARY = 575,
    PRIOR = 576,
    PRIVILEGES = 577,
    PROCEDURAL = 578,
    PROCEDURE = 579,
    PROCEDURES = 580,
    PROGRAM = 581,
    PUBLICATION = 582,
    QUOTE = 583,
    RANGE = 584,
    READ = 585,
    REAL = 586,
    REASSIGN = 587,
    RECHECK = 588,
    RECURSIVE = 589,
    REF = 590,
    REFERENCES = 591,
    REFERENCING = 592,
    REFRESH = 593,
    REINDEX = 594,
    RELATIVE_P = 595,
    RELEASE = 596,
    RENAME = 597,
    REPEATABLE = 598,
    REPLACE = 599,
    REPLICA = 600,
    RESET = 601,
    RESTART = 602,
    RESTRICT = 603,
    RETURN = 604,
    RETURNING = 605,
    RETURNS = 606,
    REVOKE = 607,
    RIGHT = 608,
    ROLE = 609,
    ROLLBACK = 610,
    ROLLUP = 611,
    ROUTINE = 612,
    ROUTINES = 613,
    ROW = 614,
    ROWS = 615,
    RULE = 616,
    SAVEPOINT = 617,
    SCHEMA = 618,
    SCHEMAS = 619,
    SCROLL = 620,
    SEARCH = 621,
    SECOND_P = 622,
    SECURITY = 623,
    SELECT = 624,
    SEQUENCE = 625,
    SEQUENCES = 626,
    SERIALIZABLE = 627,
    SERVER = 628,
    SESSION = 629,
    SESSION_USER = 630,
    SET = 631,
    SETS = 632,
    SETOF = 633,
    SHARE = 634,
    SHOW = 635,
    SIMILAR = 636,
    SIMPLE = 637,
    SKIP = 638,
    SMALLINT = 639,
    SNAPSHOT = 640,
    SOME = 641,
    SQL_P = 642,
    STABLE = 643,
    STANDALONE_P = 644,
    START = 645,
    STATEMENT = 646,
    STATISTICS = 647,
    STDIN = 648,
    STDOUT = 649,
    STORAGE = 650,
    STORED = 651,
    STRICT_P = 652,
    STRIP_P = 653,
    SUBSCRIPTION = 654,
    SUBSTRING = 655,
    SUPPORT = 656,
    SYMMETRIC = 657,
    SYSID = 658,
    SYSTEM_P = 659,
    TABLE = 660,
    TABLES = 661,
    TABLESAMPLE = 662,
    TABLESPACE = 663,
    TEMP = 664,
    TEMPLATE = 665,
    TEMPORARY = 666,
    TEXT_P = 667,
    THEN = 668,
    TIES = 669,
    TIME = 670,
    TIMESTAMP = 671,
    TO = 672,
    TRAILING = 673,
    TRANSACTION = 674,
    TRANSFORM = 675,
    TREAT = 676,
    TRIGGER = 677,
    TRIM = 678,
    TRUE_P = 679,
    TRUNCATE = 680,
    TRUSTED = 681,
    TYPE_P = 682,
    TYPES_P = 683,
    UESCAPE = 684,
    UNBOUNDED = 685,
    UNCOMMITTED = 686,
    UNENCRYPTED = 687,
    UNION = 688,
    UNIQUE = 689,
    UNKNOWN = 690,
    UNLISTEN = 691,
    UNLOGGED = 692,
    UNTIL = 693,
    UPDATE = 694,
    USER = 695,
    USING = 696,
    VACUUM = 697,
    VALID = 698,
    VALIDATE = 699,
    VALIDATOR = 700,
    VALUE_P = 701,
    VALUES = 702,
    VARCHAR = 703,
    VARIADIC = 704,
    VARYING = 705,
    VERBOSE = 706,
    VERSION_P = 707,
    VIEW = 708,
    VIEWS = 709,
    VOLATILE = 710,
    WHEN = 711,
    WHERE = 712,
    WHITESPACE_P = 713,
    WINDOW = 714,
    WITH = 715,
    WITHIN = 716,
    WITHOUT = 717,
    WORK = 718,
    WRAPPER = 719,
    WRITE = 720,
    XML_P = 721,
    XMLATTRIBUTES = 722,
    XMLCONCAT = 723,
    XMLELEMENT = 724,
    XMLEXISTS = 725,
    XMLFOREST = 726,
    XMLNAMESPACES = 727,
    XMLPARSE = 728,
    XMLPI = 729,
    XMLROOT = 730,
    XMLSERIALIZE = 731,
    XMLTABLE = 732,
    YEAR_P = 733,
    YES_P = 734,
    ZONE = 735,
    NOT_LA = 736,
    NULLS_LA = 737,
    WITH_LA = 738,
    MODE_TYPE_NAME = 739,
    MODE_PLPGSQL_EXPR = 740,
    MODE_PLPGSQL_ASSIGN1 = 741,
    MODE_PLPGSQL_ASSIGN2 = 742,
    MODE_PLPGSQL_ASSIGN3 = 743,
    UMINUS = 744
  };
#endif
/* Tokens.  */
#define IDENT 258
#define UIDENT 259
#define FCONST 260
#define SCONST 261
#define USCONST 262
#define BCONST 263
#define XCONST 264
#define Op 265
#define ICONST 266
#define PARAM 267
#define TYPECAST 268
#define DOT_DOT 269
#define COLON_EQUALS 270
#define EQUALS_GREATER 271
#define LESS_EQUALS 272
#define GREATER_EQUALS 273
#define NOT_EQUALS 274
#define ABORT_P 275
#define ABSOLUTE_P 276
#define ACCESS 277
#define ACTION 278
#define ADD_P 279
#define ADMIN 280
#define AFTER 281
#define AGGREGATE 282
#define ALL 283
#define ALSO 284
#define ALTER 285
#define ALWAYS 286
#define ANALYSE 287
#define ANALYZE 288
#define AND 289
#define ANY 290
#define ARRAY 291
#define AS 292
#define ASC 293
#define ASENSITIVE 294
#define ASSERTION 295
#define ASSIGNMENT 296
#define ASYMMETRIC 297
#define ATOMIC 298
#define AT 299
#define ATTACH 300
#define ATTRIBUTE 301
#define AUTHORIZATION 302
#define BACKWARD 303
#define BEFORE 304
#define BEGIN_P 305
#define BETWEEN 306
#define BIGINT 307
#define BINARY 308
#define BIT 309
#define BOOLEAN_P 310
#define BOTH 311
#define BREADTH 312
#define BY 313
#define CACHE 314
#define CALL 315
#define CALLED 316
#define CASCADE 317
#define CASCADED 318
#define CASE 319
#define CAST 320
#define CATALOG_P 321
#define CHAIN 322
#define CHAR_P 323
#define CHARACTER 324
#define CHARACTERISTICS 325
#define CHECK 326
#define CHECKPOINT 327
#define CLASS 328
#define CLOSE 329
#define CLUSTER 330
#define COALESCE 331
#define COLLATE 332
#define COLLATION 333
#define COLUMN 334
#define COLUMNS 335
#define COMMENT 336
#define COMMENTS 337
#define COMMIT 338
#define COMMITTED 339
#define COMPRESSION 340
#define CONCURRENTLY 341
#define CONFIGURATION 342
#define CONFLICT 343
#define CONNECTION 344
#define CONSTRAINT 345
#define CONSTRAINTS 346
#define CONTENT_P 347
#define CONTINUE_P 348
#define CONVERSION_P 349
#define COPY 350
#define COST 351
#define CREATE 352
#define CROSS 353
#define CSV 354
#define CUBE 355
#define CURRENT_P 356
#define CURRENT_CATALOG 357
#define CURRENT_DATE 358
#define CURRENT_ROLE 359
#define CURRENT_SCHEMA 360
#define CURRENT_TIME 361
#define CURRENT_TIMESTAMP 362
#define CURRENT_USER 363
#define CURSOR 364
#define CYCLE 365
#define DATA_P 366
#define DATABASE 367
#define DAY_P 368
#define DEALLOCATE 369
#define DEC 370
#define DECIMAL_P 371
#define DECLARE 372
#define DEFAULT 373
#define DEFAULTS 374
#define DEFERRABLE 375
#define DEFERRED 376
#define DEFINER 377
#define DELETE_P 378
#define DELIMITER 379
#define DELIMITERS 380
#define DEPENDS 381
#define DEPTH 382
#define DESC 383
#define DETACH 384
#define DICTIONARY 385
#define DISABLE_P 386
#define DISCARD 387
#define DISTINCT 388
#define DO 389
#define DOCUMENT_P 390
#define DOMAIN_P 391
#define DOUBLE_P 392
#define DROP 393
#define EACH 394
#define ELSE 395
#define ENABLE_P 396
#define ENCODING 397
#define ENCRYPTED 398
#define END_P 399
#define ENUM_P 400
#define ESCAPE 401
#define EVENT 402
#define EXCEPT 403
#define EXCLUDE 404
#define EXCLUDING 405
#define EXCLUSIVE 406
#define EXECUTE 407
#define EXISTS 408
#define EXPLAIN 409
#define EXPRESSION 410
#define EXTENSION 411
#define EXTERNAL 412
#define EXTRACT 413
#define FALSE_P 414
#define FAMILY 415
#define FETCH 416
#define FILTER 417
#define FINALIZE 418
#define FIRST_P 419
#define FLOAT_P 420
#define FOLLOWING 421
#define FOR 422
#define FORCE 423
#define FOREIGN 424
#define FORWARD 425
#define FREEZE 426
#define FROM 427
#define FULL 428
#define FUNCTION 429
#define FUNCTIONS 430
#define GENERATED 431
#define GLOBAL 432
#define GRANT 433
#define GRANTED 434
#define GREATEST 435
#define GROUP_P 436
#define GROUPING 437
#define GROUPS 438
#define HANDLER 439
#define HAVING 440
#define HEADER_P 441
#define HOLD 442
#define HOUR_P 443
#define IDENTITY_P 444
#define IF_P 445
#define ILIKE 446
#define IMMEDIATE 447
#define IMMUTABLE 448
#define IMPLICIT_P 449
#define IMPORT_P 450
#define IN_P 451
#define INCLUDE 452
#define INCLUDING 453
#define INCREMENT 454
#define INDEX 455
#define INDEXES 456
#define INHERIT 457
#define INHERITS 458
#define INITIALLY 459
#define INLINE_P 460
#define INNER_P 461
#define INOUT 462
#define INPUT_P 463
#define INSENSITIVE 464
#define INSERT 465
#define INSTEAD 466
#define INT_P 467
#define INTEGER 468
#define INTERSECT 469
#define INTERVAL 470
#define INTO 471
#define INVOKER 472
#define IS 473
#define ISNULL 474
#define ISOLATION 475
#define JOIN 476
#define KEY 477
#define LABEL 478
#define LANGUAGE 479
#define LARGE_P 480
#define LAST_P 481
#define LATERAL_P 482
#define LEADING 483
#define LEAKPROOF 484
#define LEAST 485
#define LEFT 486
#define LEVEL 487
#define LIKE 488
#define LIMIT 489
#define LISTEN 490
#define LOAD 491
#define LOCAL 492
#define LOCALTIME 493
#define LOCALTIMESTAMP 494
#define LOCATION 495
#define LOCK_P 496
#define LOCKED 497
#define LOGGED 498
#define MAPPING 499
#define MATCH 500
#define MATCHED 501
#define MATERIALIZED 502
#define MAXVALUE 503
#define MERGE 504
#define METHOD 505
#define MINUTE_P 506
#define MINVALUE 507
#define MODE 508
#define MONTH_P 509
#define MOVE 510
#define NAME_P 511
#define NAMES 512
#define NATIONAL 513
#define NATURAL 514
#define NCHAR 515
#define NEW 516
#define NEXT 517
#define NFC 518
#define NFD 519
#define NFKC 520
#define NFKD 521
#define NO 522
#define NONE 523
#define NORMALIZE 524
#define NORMALIZED 525
#define NOT 526
#define NOTHING 527
#define NOTIFY 528
#define NOTNULL 529
#define NOWAIT 530
#define NULL_P 531
#define NULLIF 532
#define NULLS_P 533
#define NUMERIC 534
#define OBJECT_P 535
#define OF 536
#define OFF 537
#define OFFSET 538
#define OIDS 539
#define OLD 540
#define ON 541
#define ONLY 542
#define OPERATOR 543
#define OPTION 544
#define OPTIONS 545
#define OR 546
#define ORDER 547
#define ORDINALITY 548
#define OTHERS 549
#define OUT_P 550
#define OUTER_P 551
#define OVER 552
#define OVERLAPS 553
#define OVERLAY 554
#define OVERRIDING 555
#define OWNED 556
#define OWNER 557
#define PARALLEL 558
#define PARAMETER 559
#define PARSER 560
#define PARTIAL 561
#define PARTITION 562
#define PASSING 563
#define PASSWORD 564
#define PGPOOL 565
#define PLACING 566
#define PLANS 567
#define POLICY 568
#define POSITION 569
#define PRECEDING 570
#define PRECISION 571
#define PRESERVE 572
#define PREPARE 573
#define PREPARED 574
#define PRIMARY 575
#define PRIOR 576
#define PRIVILEGES 577
#define PROCEDURAL 578
#define PROCEDURE 579
#define PROCEDURES 580
#define PROGRAM 581
#define PUBLICATION 582
#define QUOTE 583
#define RANGE 584
#define READ 585
#define REAL 586
#define REASSIGN 587
#define RECHECK 588
#define RECURSIVE 589
#define REF 590
#define REFERENCES 591
#define REFERENCING 592
#define REFRESH 593
#define REINDEX 594
#define RELATIVE_P 595
#define RELEASE 596
#define RENAME 597
#define REPEATABLE 598
#define REPLACE 599
#define REPLICA 600
#define RESET 601
#define RESTART 602
#define RESTRICT 603
#define RETURN 604
#define RETURNING 605
#define RETURNS 606
#define REVOKE 607
#define RIGHT 608
#define ROLE 609
#define ROLLBACK 610
#define ROLLUP 611
#define ROUTINE 612
#define ROUTINES 613
#define ROW 614
#define ROWS 615
#define RULE 616
#define SAVEPOINT 617
#define SCHEMA 618
#define SCHEMAS 619
#define SCROLL 620
#define SEARCH 621
#define SECOND_P 622
#define SECURITY 623
#define SELECT 624
#define SEQUENCE 625
#define SEQUENCES 626
#define SERIALIZABLE 627
#define SERVER 628
#define SESSION 629
#define SESSION_USER 630
#define SET 631
#define SETS 632
#define SETOF 633
#define SHARE 634
#define SHOW 635
#define SIMILAR 636
#define SIMPLE 637
#define SKIP 638
#define SMALLINT 639
#define SNAPSHOT 640
#define SOME 641
#define SQL_P 642
#define STABLE 643
#define STANDALONE_P 644
#define START 645
#define STATEMENT 646
#define STATISTICS 647
#define STDIN 648
#define STDOUT 649
#define STORAGE 650
#define STORED 651
#define STRICT_P 652
#define STRIP_P 653
#define SUBSCRIPTION 654
#define SUBSTRING 655
#define SUPPORT 656
#define SYMMETRIC 657
#define SYSID 658
#define SYSTEM_P 659
#define TABLE 660
#define TABLES 661
#define TABLESAMPLE 662
#define TABLESPACE 663
#define TEMP 664
#define TEMPLATE 665
#define TEMPORARY 666
#define TEXT_P 667
#define THEN 668
#define TIES 669
#define TIME 670
#define TIMESTAMP 671
#define TO 672
#define TRAILING 673
#define TRANSACTION 674
#define TRANSFORM 675
#define TREAT 676
#define TRIGGER 677
#define TRIM 678
#define TRUE_P 679
#define TRUNCATE 680
#define TRUSTED 681
#define TYPE_P 682
#define TYPES_P 683
#define UESCAPE 684
#define UNBOUNDED 685
#define UNCOMMITTED 686
#define UNENCRYPTED 687
#define UNION 688
#define UNIQUE 689
#define UNKNOWN 690
#define UNLISTEN 691
#define UNLOGGED 692
#define UNTIL 693
#define UPDATE 694
#define USER 695
#define USING 696
#define VACUUM 697
#define VALID 698
#define VALIDATE 699
#define VALIDATOR 700
#define VALUE_P 701
#define VALUES 702
#define VARCHAR 703
#define VARIADIC 704
#define VARYING 705
#define VERBOSE 706
#define VERSION_P 707
#define VIEW 708
#define VIEWS 709
#define VOLATILE 710
#define WHEN 711
#define WHERE 712
#define WHITESPACE_P 713
#define WINDOW 714
#define WITH 715
#define WITHIN 716
#define WITHOUT 717
#define WORK 718
#define WRAPPER 719
#define WRITE 720
#define XML_P 721
#define XMLATTRIBUTES 722
#define XMLCONCAT 723
#define XMLELEMENT 724
#define XMLEXISTS 725
#define XMLFOREST 726
#define XMLNAMESPACES 727
#define XMLPARSE 728
#define XMLPI 729
#define XMLROOT 730
#define XMLSERIALIZE 731
#define XMLTABLE 732
#define YEAR_P 733
#define YES_P 734
#define ZONE 735
#define NOT_LA 736
#define NULLS_LA 737
#define WITH_LA 738
#define MODE_TYPE_NAME 739
#define MODE_PLPGSQL_EXPR 740
#define MODE_PLPGSQL_ASSIGN1 741
#define MODE_PLPGSQL_ASSIGN2 742
#define MODE_PLPGSQL_ASSIGN3 743
#define UMINUS 744

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 266 "gram_minimal.y" /* yacc.c:1909  */

	core_YYSTYPE core_yystype;
	/* these fields must match core_YYSTYPE: */
	int			ival;
	char	   *str;
	const char *keyword;

	char		chr;
	bool		boolean;
	JoinType	jtype;
	DropBehavior dbehavior;
	OnCommitAction oncommit;
	List	   *list;
	Node	   *node;
	ObjectType	objtype;
	TypeName   *typnam;
	FunctionParameter *fun_param;
	FunctionParameterMode fun_param_mode;
	ObjectWithArgs *objwithargs;
	DefElem	   *defelt;
	SortBy	   *sortby;
	WindowDef  *windef;
	JoinExpr   *jexpr;
	IndexElem  *ielem;
	StatsElem  *selem;
	Alias	   *alias;
	RangeVar   *range;
	IntoClause *into;
	WithClause *with;
	InferClause	*infer;
	OnConflictClause *onconflict;
	A_Indices  *aind;
	ResTarget  *target;
	struct PrivTarget *privtarget;
	AccessPriv *accesspriv;
	struct ImportQual *importqual;
	InsertStmt *istmt;
	VariableSetStmt *vsetstmt;
	PartitionElem *partelem;
	PartitionSpec *partspec;
	PartitionBoundSpec *partboundspec;
	RoleSpec   *rolespec;
	PublicationObjSpec *publicationobjectspec;
	struct SelectLimit *selectlimit;
	SetQuantifier setquantifier;
	struct GroupClause *groupclause;
	MergeWhenClause *mergewhen;
	struct KeyActions *keyactions;
	struct KeyAction *keyaction;

#line 1083 "gram_minimal.h" /* yacc.c:1909  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif

/* Location type.  */
#if ! defined YYLTYPE && ! defined YYLTYPE_IS_DECLARED
typedef struct YYLTYPE YYLTYPE;
struct YYLTYPE
{
  int first_line;
  int first_column;
  int last_line;
  int last_column;
};
# define YYLTYPE_IS_DECLARED 1
# define YYLTYPE_IS_TRIVIAL 1
#endif



int minimal_base_yyparse (core_yyscan_t yyscanner);

#endif /* !YY_MINIMAL_BASE_YY_GRAM_MINIMAL_H_INCLUDED  */
