# Hidden Legends - Treasure Hunt Management System

A command-line application for creating and managing digital treasure hunts.

## Overview

Hidden Legends is a treasure hunt management system that allows organizers to create hunts, add treasures with GPS coordinates, clues, and values, and track participation. The system stores treasure data in separate directories for each hunt and maintains operation logs.

## Features

### Phase 1: Basic Functionality
- **Create Treasure Hunts**: Organize multiple independent treasure hunts
- **Add Treasures**: Add treasures with unique IDs, GPS coordinates, clues, and values
- **List Treasures**: View all treasures in a particular hunt
- **View Treasure Details**: Get complete information about a specific treasure
- **Remove Treasures**: Remove individual treasures from a hunt
- **Remove Hunts**: Delete entire treasure hunts
- **Automatic Logging**: All operations are logged with timestamps
- **Symlink Creation**: Easy access to log files through symbolic links

### Phase 2: Process & Signal Communication
- **Interactive Hub Interface**: A separate program for interacting with the treasure system
- **Background Monitor Process**: Monitor hunts and treasures through a separate process
- **Signal-Based Communication**: Inter-process communication using UNIX signals
- **Hunt Statistics**: View overall statistics about all hunts in the system
- **Process Coordination**: Proper handling of process termination and cleanup

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

2. Compile all the source code:
   ```
   ./compile.sh
   ```

## Usage

### Phase 1: Direct Treasure Management

#### Adding a Treasure to a Hunt

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

#### Listing Treasures in a Hunt

```
./treasure_manager --list <hunt_id>
```

#### Viewing a Specific Treasure

```
./treasure_manager --view <hunt_id> <treasure_id>
```

#### Removing a Treasure

```
./treasure_manager --remove_treasure <hunt_id> <treasure_id>
```

#### Removing an Entire Hunt

```
./treasure_manager --remove_hunt <hunt_id>
```

### Phase 2: Interactive Hub Interface

Run the interactive hub:
```
./treasure_hub
```

Available commands in the hub:
- `start_monitor`: Start a background process that monitors the hunts
- `list_hunts`: Show all hunts and their total treasure count
- `list_treasures`: Show all treasures in a specific hunt
- `view_treasure`: Show details of a specific treasure
- `stop_monitor`: Stop the monitoring process
- `exit`: Exit the hub program (monitor must be stopped first)

## File Structure

```
<hunt_id>/
  ├── treasures.dat     # Binary file containing treasure data
  └── logged_hunt       # Log file of all operations for this hunt
logged_hunt-<hunt_id>   # Symlink to the hunt's log file
command.tmp             # Temporary file for command communication
args.tmp                # Temporary file for command arguments
```

## Implementation Details

- Treasures are stored in binary format using a struct
- Each hunt has its own directory containing treasure data and logs
- All operations are timestamped and logged
- Symbolic links provide quick access to log files
- Inter-process communication uses Unix signals
- Monitor process interprets commands through temporary files
