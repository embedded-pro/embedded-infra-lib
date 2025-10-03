#!/usr/bin/env python3
"""
Mirror Sync Script
This script handles the complex logic for syncing with a mirror repository
"""

import argparse
import os
import sys
from typing import Optional


def create_summary(status: str, message: str, config: dict) -> None:
    """Create GitHub step summary"""
    summary_content = f"""## Mirror Sync Results {status}

{message}

**Configuration:**
- Mirror URL: {config['mirror_url']}
- Mirror Branch: {config['mirror_branch']}
- Target Branch: {config['target_branch']}
"""
    
    with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as f:
        f.write(summary_content)


def handle_sync_results(sync_output: str, config: dict) -> int:
    """Handle sync results based on action outputs"""
    if "sync-skipped=true" in sync_output:
        print("ℹ️ Sync skipped due to no recent activity in mirror repository")
        create_summary("ℹ️", f"""Sync skipped due to no recent activity:

- **Activity Period**: {config['activity_period']}

No commits found in the specified period.""", config)
        return 0
        
    elif "changes-detected=true" in sync_output:
        # Extract PR info
        pr_number = None
        pr_url = None
        for line in sync_output.split('\n'):
            if "pr-number=" in line:
                pr_number = line.split('=', 1)[1]
            elif "pr-url=" in line:
                pr_url = line.split('=', 1)[1]
        
        print(f"✅ Successfully created PR #{pr_number}")
        print(f"🔗 PR URL: {pr_url}")
        
        create_summary("✅", f"""Successfully synced changes from mirror repository:

- **PR Number**: #{pr_number}
- **PR URL**: {pr_url}

Please review and merge the PR when ready.""", config)
        return 0
        
    else:
        print("ℹ️ No changes detected from mirror repository")
        create_summary("ℹ️", """No changes detected from mirror repository.

The mirror repository is up to date with the target branch.""", config)
        return 0


def handle_failure(config: dict) -> None:
    """Handle failures"""
    print("❌ Mirror sync failed")
    create_summary("❌", """The mirror sync workflow failed. Possible causes:

- **Authentication issues**: Check GITLAB_TOKEN_USERNAME and GITLAB_TOKEN_PASSWORD secrets
- **Network issues**: Mirror repository might be unreachable
- **Merge conflicts**: Manual resolution may be required
- **Permission issues**: Check repository permissions

Please check the workflow logs for more details.""", config)


def setup_environment(config: dict) -> None:
    """Set environment variables for the GitHub Action"""
    github_env = os.environ.get('GITHUB_ENV')
    if github_env:
        with open(github_env, 'a') as f:
            f.write(f"MIRROR_URL={config['mirror_url']}\n")
            f.write(f"MIRROR_BRANCH={config['mirror_branch']}\n")
            f.write(f"TARGET_BRANCH={config['target_branch']}\n")


def main():
    """Main execution function"""
    parser = argparse.ArgumentParser(description='Mirror Sync Script')
    parser.add_argument('--mirror-url', required=True,
                       help='URL of the mirror repository')
    parser.add_argument('--mirror-branch', default='main',
                       help='Branch to sync from mirror (default: main)')
    parser.add_argument('--target-branch', default='main',
                       help='Target branch in this repository (default: main)')
    parser.add_argument('--sync-branch-prefix', default='sync-mirror',
                       help='Prefix for sync branches (default: sync-mirror)')
    parser.add_argument('--activity-period', default='1 month ago',
                       help='Period to check for activity (default: 1 month ago)')
    parser.add_argument('--is-scheduled', action='store_true',
                       help='Whether this is a scheduled run')
    parser.add_argument('--force-sync', action='store_true',
                       help='Force sync even without recent activity')
    
    args = parser.parse_args()
    
    config = {
        'mirror_url': args.mirror_url,
        'mirror_branch': args.mirror_branch,
        'target_branch': args.target_branch,
        'sync_branch_prefix': args.sync_branch_prefix,
        'activity_period': args.activity_period,
        'is_scheduled': args.is_scheduled,
        'force_sync': args.force_sync
    }
    
    print("🔄 Starting mirror sync process...")
    print(f"📂 Mirror URL: {config['mirror_url']}")
    print(f"🌿 Mirror branch: {config['mirror_branch']}")
    print(f"🎯 Target branch: {config['target_branch']}")
    print(f"📅 Scheduled run: {config['is_scheduled']}")
    
    try:
        # Set up environment variables for the GitHub Action
        setup_environment(config)
        
        # Determine check activity setting
        check_activity = config['is_scheduled'] and not config['force_sync']
        
        print("📋 Action inputs:")
        print(f"  - mirror-url: {config['mirror_url']}")
        print(f"  - mirror-branch: {config['mirror_branch']}")
        print(f"  - target-branch: {config['target_branch']}")
        print(f"  - sync-branch-prefix: {config['sync_branch_prefix']}")
        print(f"  - check-activity: {check_activity}")
        print(f"  - activity-period: {config['activity_period']}")
        print(f"  - force-sync: {config['force_sync']}")
        
        print("🚀 Environment setup completed successfully")
        
    except Exception as e:
        print(f"❌ Setup failed: {e}")
        handle_failure(config)
        sys.exit(1)


if __name__ == "__main__":
    main()