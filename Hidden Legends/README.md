# Hidden Legends - Treasure Hunt Management System

A command-line application for creating and managing digital treasure hunts.

## Overview

Hidden Legends is a treasure hunt management system that allows organizers to create hunts, add treasures with GPS coordinates, clues, and values, and track participation. The system stores treasure data in separate directories for each hunt and maintains operation logs.

## Features

- **Create Treasure Hunts**: Organize multiple independent treasure hunts
- **Add Treasures**: Add treasures with unique IDs, GPS coordinates, clues, and values
- **List Treasures**: View all treasures in a particular hunt
- **View Treasure Details**: Get complete information about a specific treasure
- **Remove Treasures**: Remove individual treasures from a hunt
- **Remove Hunts**: Delete entire treasure hunts
- **Automatic Logging**: All operations are logged with timestamps
- **Symlink Creation**: Easy access to log files through symbolic links

## Requirements

- UNIX-like operating system (Linux, macOS)
- C compiler (gcc recommended)
- Basic command-line knowledge

## Installation

1. Clone the repository:
   ```
   git clone https://github.com/Davide-glitch/hidden-legends.git
   cd hidden-legends
   ```

2. Compile the source code:
   ```
   gcc treasure_manager.c -o treasure_manager
   ```

3. Make the binary executable:
   ```
   chmod +x treasure_manager
   ```

## Usage

### Adding a Treasure to a Hunt

```
./treasure_manager --add <hunt_id>
```

Example:
```
./treasure_manager --add downtown_hunt
```

You'll be prompted to enter:
- Treasure ID
- Username (who placed the treasure)
- Latitude and longitude
- Clue to find the treasure
- Value of the treasure

### Listing Treasures in a Hunt

```
./treasure_manager --list <hunt_id>
```

Example:
```
./treasure_manager --list downtown_hunt
```

This displays all treasures in the specified hunt along with file statistics.

### Viewing a Specific Treasure

```
./treasure_manager --view <hunt_id> <treasure_id>
```

Example:
```
./treasure_manager --view downtown_hunt treasure42
```

### Removing a Treasure

```
./treasure_manager --remove_treasure <hunt_id> <treasure_id>
```

Example:
```
./treasure_manager --remove_treasure downtown_hunt treasure42
```

### Removing an Entire Hunt

```
./treasure_manager --remove_hunt <hunt_id>
```

Example:
```
./treasure_manager --remove_hunt downtown_hunt
```

## File Structure

```
<hunt_id>/
  ├── treasures.dat     # Binary file containing treasure data
  └── logged_hunt       # Log file of all operations for this hunt
logged_hunt-<hunt_id>   # Symlink to the hunt's log file
```

## Implementation Details

- Treasures are stored in binary format using a struct
- Each hunt has its own directory containing treasure data and logs
- All operations are timestamped and logged
- Symbolic links provide quick access to log files

## Security Considerations

- The application performs basic directory and file operations
- File permissions are set to 0644 (read/write for owner, read-only for others)
- Directory permissions are set to 0755 (read/write/execute for owner, read/execute for others)
