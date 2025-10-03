#!/usr/bin/env python3
"""
Handle sync results script
Process the outputs from the sync-mirror action
"""

import argparse
import os
import sys


def create_summary(status: str, content: str, config: dict) -> None:
    """Create GitHub step summary"""
    summary = f"""## Mirror Sync Results {status}

{content}

**Configuration:**
- Mirror URL: {config['mirror_url']}
- Mirror Branch: {config['mirror_branch']}
- Target Branch: {config['target_branch']}
"""
    
    with open(os.environ['GITHUB_STEP_SUMMARY'], 'a') as f:
        f.write(summary)


def main():
    """Main execution function"""
    parser = argparse.ArgumentParser(description='Handle sync results')
    parser.add_argument('--sync-skipped', default='false',
                       help='Whether sync was skipped')
    parser.add_argument('--changes-detected', default='false',
                       help='Whether changes were detected')
    parser.add_argument('--pr-number', help='PR number if created')
    parser.add_argument('--pr-url', help='PR URL if created')
    parser.add_argument('--activity-period', default='1 month ago',
                       help='Activity period that was checked')
    
    args = parser.parse_args()
    
    # Get config from environment variables
    config = {
        'mirror_url': os.environ.get('MIRROR_URL', ''),
        'mirror_branch': os.environ.get('MIRROR_BRANCH', ''),
        'target_branch': os.environ.get('TARGET_BRANCH', '')
    }
    
    try:
        if args.sync_skipped.lower() == 'true':
            print("ℹ️ Sync skipped due to no recent activity in mirror repository")
            create_summary("ℹ️", f"""Sync skipped due to no recent activity:

- **Activity Period**: {args.activity_period}

No commits found in the specified period.""", config)

        elif args.changes_detected.lower() == 'true':
            print(f"✅ Successfully created PR #{args.pr_number}")
            print(f"🔗 PR URL: {args.pr_url}")
            create_summary("✅", f"""Successfully synced changes from mirror repository:

- **PR Number**: #{args.pr_number}
- **PR URL**: {args.pr_url}

Please review and merge the PR when ready.""", config)

        else:
            print("ℹ️ No changes detected from mirror repository")
            create_summary("ℹ️", """No changes detected from mirror repository.

The mirror repository is up to date with the target branch.""", config)
            
    except Exception as e:
        print(f"❌ Error handling results: {e}")
        sys.exit(1)


if __name__ == "__main__":
    main()