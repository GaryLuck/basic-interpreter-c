# READ and DATA Statement Implementation

## Summary
The BASIC interpreter has been enhanced to support READ and DATA statements, which were fundamental features of early 1970s BASIC programming languages.

## Implementation Details

### Data Storage
- Added a `data_values` array to store numeric values from DATA statements (max 1000 values)
- Added `data_count` to track the number of data values loaded
- Added `data_index` to track the current position for READ operations

### DATA Statement
- Parses comma-separated numeric values in DATA statements
- Example: `DATA 10, 20, 30, 40, 50`
- Multiple DATA statements are supported and their values are concatenated
- DATA statements are collected during program initialization before execution

### READ Statement
- Reads one or more values from the data pool into variables
- Example: `READ X, Y, Z`
- Variables can be single letters (A-Z) or letter with digit (A0-Z9)
- Returns a runtime error if attempting to read beyond available data

## Usage Examples

### Simple READ/DATA
```basic
10 DATA 10, 20, 30
20 READ X
30 PRINT X
40 END
```

### Multiple Variables
```basic
10 DATA 5, 10, 15
20 READ A, B, C
30 PRINT A
40 PRINT B
50 PRINT C
60 END
```

### Multiple DATA Statements
```basic
10 DATA 100, 200
20 DATA 300, 400, 500
30 READ X, Y, Z, W, V
40 FOR I = 1 TO 5
50 PRINT X + (I-1)*100
60 NEXT I
70 END
```

### Combined with FOR Loops
```basic
10 DATA 1, 2, 3, 4, 5
20 FOR I = 1 TO 5
30 READ N
40 PRINT N
50 NEXT I
60 END
```

## Test Files Created
- `examples/read_data_demo.bas` - Basic READ/DATA demonstration
- `examples/read_multiple.bas` - Multiple READ with multiple variables
- `examples/multi_data.bas` - Multiple DATA statements
- `examples/read_loop.bas` - READ combined with FOR loops
- `examples/scores.bas` - Realistic example with data
- `examples/read_error.bas` - Error handling demonstration

## Error Handling
- Returns "Runtime error: READ beyond available DATA" if attempting to read more values than available
