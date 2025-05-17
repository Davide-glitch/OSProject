# Hidden Legends - Treasure Hunt Management System

A command-line application for creating and managing digital treasure hunts.

## Overview

Hidden Legends is a treasure hunt management system that allows organizers to create hunts, add treasures with GPS coordinates, clues, and values, and track participation. The system stores treasure data in separate directories for each hunt and maintains operation logs. An interactive hub allows for monitoring and advanced operations like score calculation.

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
- **Interactive Hub Interface**: A separate program (`treasure_hub`) for interacting with the treasure system.
- **Background Monitor Process**: `treasure_monitor` runs in the background, managed by the hub.
- **Signal-Based Communication**: Hub and monitor communicate using UNIX signals.
- **Hunt Statistics**: View overall statistics about all hunts in the system via the hub.
- **Process Coordination**: Proper handling of process termination and cleanup.

### Phase 3: Pipes, Redirects & External Integration
- **Pipe Communication**: Monitor process sends results back to the hub via pipes.
- **`calculate_score` Command**: A new command in the hub to calculate user scores for each hunt.
- **External Score Calculator**: An external program (`score_calculator`) is invoked by the hub to compute scores for each hunt.
- **Output Redirection**: Score calculator's output is piped back to the hub for display.

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

### Phase 1: Direct Treasure Management (`./treasure_manager`)

(Commands remain the same: `--add`, `--list`, `--view`, `--remove_treasure`, `--remove_hunt`)

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

### Interactive Hub Interface (`./treasure_hub`)

Run the interactive hub:
```
./treasure_hub
```

Available commands in the hub:
- `start_monitor`: Start a background process that monitors the hunts.
- `list_hunts`: Show all hunts and their total treasure count (results via pipe from monitor).
- `list_treasures`: Show all treasures in a specific hunt (results via pipe from monitor).
- `view_treasure`: Show details of a specific treasure (results via pipe from monitor).
- `calculate_score`: Calculates and displays user scores for all existing hunts using an external program.
- `stop_monitor`: Stop the monitoring process.
- `exit`: Exit the hub program (monitor must be stopped first).

## File Structure

```
<hunt_id>/
  ├── treasures.dat     # Binary file containing treasure data
  └── logged_hunt       # Log file of all operations for this hunt
logged_hunt-<hunt_id>   # Symlink to the hunt's log file
command.tmp             # Temporary file for command communication (hub to monitor)
args.tmp                # Temporary file for command arguments (hub to monitor)
treasure_manager        # Executable for direct treasure management
treasure_hub            # Executable for interactive hub interface
treasure_monitor        # Executable for background monitoring process
score_calculator        # Executable for calculating user scores
```

## Implementation Details

- Treasures are stored in binary format using a struct.
- Each hunt has its own directory containing treasure data and logs.
- All operations are timestamped and logged by `treasure_manager`.
- Symbolic links provide quick access to log files.
- Inter-process communication:
    - Hub to Monitor: Signals and temporary files (`command.tmp`, `args.tmp`).
    - Monitor to Hub: Pipes for command results.
    - Hub to Score Calculator: Pipes for output.
- Monitor process interprets commands and sends results back.
- `score_calculator` is an external program invoked for per-hunt score calculation.
