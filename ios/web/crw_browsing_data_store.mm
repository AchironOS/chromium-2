// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/public/crw_browsing_data_store.h"

#import <Foundation/Foundation.h>

#include "base/ios/ios_util.h"
#import "base/ios/weak_nsobject.h"
#include "base/logging.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/web/browsing_data_managers/crw_cookie_browsing_data_manager.h"
#include "ios/web/public/active_state_manager.h"
#include "ios/web/public/browser_state.h"

namespace web {
NSString* const kBrowsingDataTypeCookies = @"kBrowsingDataTypeCookies";
}  // namespace web

namespace {
// Represents the type of operations that a CRWBrowsingDataStore can perform.
enum OperationType {
  STASH = 0,
  RESTORE,
  REMOVE
};
}  // namespace

@interface CRWBrowsingDataStore ()
// Returns a serial queue on which stash and restore operations can be scheduled
// to be run. All stash/restore operations need to be run on the same queue
// hence it is shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForStashAndRestoreOperations;
// Returns a concurrent queue on which remove operations can be scheduled to be
// run. All remove operations need to be run on the same queue hence it is
// shared with all CRWBrowsingDataStores.
+ (NSOperationQueue*)operationQueueForRemoveOperations;

// The array of all browsing data managers. Must be accessed from the main
// thread.
@property(nonatomic, readonly) NSArray* allBrowsingDataManagers;
// Returns the |CRWBrowsingDataManager| that manages the browsing data for
// |browsingDataType|.
- (id<CRWBrowsingDataManager>)browsingDataManagerForBrowsingDataType:
        (NSString*)browsingDataType;
// Returns an array of browsing data managers for all the |browsingDataType|.
- (NSArray*)browsingDataManagersForBrowsingDataTypes:(NSSet*)browsingDataTypes;

// Redefined to be read-write. Must be called from the main thread.
@property(nonatomic, assign) CRWBrowsingDataStoreMode mode;
// Sets the mode iff there are no more stash or restore operations that are
// still pending. |mode| can only be either |ACTIVE| or |INACTIVE|.
- (void)setModeIfNotStashingOrRestoring:(CRWBrowsingDataStoreMode)mode;

// Performs operations of type |operationType| on each of the
// |browsingDataManagers|.
// The kind of operations are:
// 1) STASH: Stash operation involves stashing browsing data that web views
// (UIWebViews and WKWebViews) create to the associated BrowserState's state
// path.
// 2) RESTORE: Restore operation involves restoring browsing data from the
// associated BrowserState's state path so that web views (UIWebViews and
// WKWebViews) can read from them.
// 3) REMOVE: Remove operation involves removing the data that web views
// (UIWebViews and WKWebViews) create.
// Precondition: There must be no web views associated with the BrowserState.
// |completionHandler| is called on the main thread and cannot be nil.
- (void)performOperationWithType:(OperationType)operationType
            browsingDataManagers:(NSArray*)browsingDataManagers
               completionHandler:(ProceduralBlock)completionHandler;

// Creates an NSOperation that calls |selector| on all the
// |browsingDataManagers|. |selector| needs to be one of the methods in
// CRWBrowsingDataManager. The created NSOperation will have a completionBlock
// of |completionHandler|.
- (NSOperation*)
    newOperationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                selector:(SEL)selector
                       completionHandler:(ProceduralBlock)completionHandler;
// Enqueues |operation| to be run on |queue|. All operations are serialized to
// be run one after another.
- (void)addOperation:(NSOperation*)operation toQueue:(NSOperationQueue*)queue;
@end

@implementation CRWBrowsingDataStore {
  web::BrowserState* _browserState;  // Weak, owns this object.
  // The mode of the CRWBrowsingDataStore.
  CRWBrowsingDataStoreMode _mode;
  // The dictionary that maps a browsing data type to its
  // CRWBrowsingDataManager.
  base::scoped_nsobject<NSDictionary> _browsingDataTypeMap;
  // The last operation that was enqueued to be run. Can be stash, restore or a
  // delete operation.
  base::scoped_nsobject<NSOperation> _lastDispatchedOperation;
  // The last stash or restore operations that was dispatched to be run.
  base::scoped_nsobject<NSOperation> _lastDispatchedStashOrRestoreOperation;
}

+ (NSOperationQueue*)operationQueueForStashAndRestoreOperations {
  static dispatch_once_t onceToken = 0;
  static NSOperationQueue* operationQueueForStashAndRestoreOperations = nil;
  dispatch_once(&onceToken, ^{
    operationQueueForStashAndRestoreOperations =
        [[NSOperationQueue alloc] init];
    [operationQueueForStashAndRestoreOperations
        setMaxConcurrentOperationCount:1U];
    if (base::ios::IsRunningOnIOS8OrLater()) {
      [operationQueueForStashAndRestoreOperations
          setQualityOfService:NSQualityOfServiceUserInteractive];
    }
  });
  return operationQueueForStashAndRestoreOperations;
}

+ (NSOperationQueue*)operationQueueForRemoveOperations {
  static dispatch_once_t onceToken = 0;
  static NSOperationQueue* operationQueueForRemoveOperations = nil;
  dispatch_once(&onceToken, ^{
    operationQueueForRemoveOperations = [[NSOperationQueue alloc] init];
    [operationQueueForRemoveOperations
        setMaxConcurrentOperationCount:NSUIntegerMax];
    if (base::ios::IsRunningOnIOS8OrLater()) {
      [operationQueueForRemoveOperations
          setQualityOfService:NSQualityOfServiceUserInitiated];
    }
  });
  return operationQueueForRemoveOperations;
}

- (instancetype)initWithBrowserState:(web::BrowserState*)browserState {
  self = [super init];
  if (self) {
    DCHECK(web::BrowserState::GetActiveStateManager(browserState)->IsActive());
    DCHECK([NSThread isMainThread]);
    // TODO(shreyasv): Instantiate the necessary CRWBrowsingDataManagers that
    // are encapsulated within this class. crbug.com/480654.
    _browserState = browserState;
    _mode = ACTIVE;
    // TODO(shreyasv): If the creation of CRWBrowsingDataManagers turns out to
    // be an expensive operations re-visit this with a lazy-evaluation approach.
    base::scoped_nsobject<CRWCookieBrowsingDataManager>
        cookieBrowsingDataManager([[CRWCookieBrowsingDataManager alloc]
            initWithBrowserState:browserState]);
    _browsingDataTypeMap.reset([@{
      web::kBrowsingDataTypeCookies : cookieBrowsingDataManager,
    } retain]);
  }
  return self;
}

- (NSString*)description {
  NSString* format = @"<%@: %p; hasPendingOperations = { %@ }>";
  NSString* hasPendingOperationsString =
      [self hasPendingOperations] ? @"YES" : @"NO";
  NSString* result =
      [NSString stringWithFormat:format, NSStringFromClass(self.class), self,
                                 hasPendingOperationsString];
  return result;
}

+ (NSSet*)allBrowsingDataTypes {
  return [[[NSSet alloc]
      initWithObjects:web::kBrowsingDataTypeCookies, nil] autorelease];
}

- (NSArray*)allBrowsingDataManagers {
  DCHECK([NSThread isMainThread]);
  return [_browsingDataTypeMap allValues];
}

- (id<CRWBrowsingDataManager>)browsingDataManagerForBrowsingDataType:
        (NSString*)browsingDataType {
  return [_browsingDataTypeMap objectForKey:browsingDataType];
}

- (NSArray*)browsingDataManagersForBrowsingDataTypes:(NSSet*)browsingDataTypes {
  return [_browsingDataTypeMap objectsForKeys:[browsingDataTypes allObjects]
                               notFoundMarker:[NSNull null]];
}

- (CRWBrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  return _mode;
}

- (void)setMode:(CRWBrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  _mode = mode;
}

- (void)setModeIfNotStashingOrRestoring:(CRWBrowsingDataStoreMode)mode {
  DCHECK([NSThread isMainThread]);
  DCHECK_NE(SYNCHRONIZING, mode);
  if ([_lastDispatchedStashOrRestoreOperation isFinished]) {
    _mode = mode;
  }
}

- (void)makeActiveWithCompletionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);

  base::WeakNSObject<CRWBrowsingDataStore> weakSelf(self);
  [self performOperationWithType:RESTORE
            browsingDataManagers:[self allBrowsingDataManagers]
               completionHandler:^{
                 [weakSelf setModeIfNotStashingOrRestoring:ACTIVE];
                 if (completionHandler) {
                   completionHandler();
                 }
               }];
}

- (void)makeInactiveWithCompletionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);

  base::WeakNSObject<CRWBrowsingDataStore> weakSelf(self);
  [self performOperationWithType:STASH
            browsingDataManagers:[self allBrowsingDataManagers]
               completionHandler:^{
                 [weakSelf setModeIfNotStashingOrRestoring:INACTIVE];
                 if (completionHandler) {
                   completionHandler();
                 }
               }];
}

- (void)removeDataOfTypes:(NSSet*)browsingDataTypes
        completionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);

  NSArray* browsingDataManagers =
      [self browsingDataManagersForBrowsingDataTypes:browsingDataTypes];
  [self performOperationWithType:REMOVE
            browsingDataManagers:browsingDataManagers
               completionHandler:^{
                 // Since this may be called on a background thread, bounce to
                 // the main thread.
                 dispatch_async(dispatch_get_main_queue(), completionHandler);
               }];
}

- (BOOL)hasPendingOperations {
  if (!_lastDispatchedOperation) {
    return NO;
  }
  return ![_lastDispatchedOperation isFinished];
}

- (void)performOperationWithType:(OperationType)operationType
            browsingDataManagers:(NSArray*)browsingDataManagers
               completionHandler:(ProceduralBlock)completionHandler {
  DCHECK([NSThread isMainThread]);
  DCHECK(completionHandler);

  SEL selector = nullptr;
  switch (operationType) {
    case STASH:
      selector = @selector(stashData);
      break;
    case RESTORE:
      selector = @selector(restoreData);
      break;
    case REMOVE:
      selector = @selector(removeData);
      break;
    default:
      NOTREACHED();
      break;
  };

  id callCompletionHandlerOnMainThread = ^{
    // This can be called on a background thread, hence the need to bounce to
    // the main thread.
    dispatch_async(dispatch_get_main_queue(), ^{
      completionHandler();
    });
  };
  base::scoped_nsobject<NSOperation> operation([self
      newOperationWithBrowsingDataManagers:browsingDataManagers
                                  selector:selector
                         completionHandler:callCompletionHandlerOnMainThread]);

  if (operationType == RESTORE || operationType == STASH) {
    _mode = SYNCHRONIZING;
    _lastDispatchedStashOrRestoreOperation.reset([operation retain]);
  }

  NSOperationQueue* queue = nil;
  switch (operationType) {
    case STASH:
    case RESTORE:
      queue = [CRWBrowsingDataStore operationQueueForStashAndRestoreOperations];
      break;
    case REMOVE:
      queue = [CRWBrowsingDataStore operationQueueForRemoveOperations];
      break;
    default:
      NOTREACHED();
      break;
  }

  [self addOperation:operation toQueue:queue];
}

- (NSOperation*)
    newOperationWithBrowsingDataManagers:(NSArray*)browsingDataManagers
                                selector:(SEL)selector
                       completionHandler:(ProceduralBlock)completionHandler {
  NSBlockOperation* operation = [[NSBlockOperation alloc] init];
  for (id<CRWBrowsingDataManager> manager : browsingDataManagers) {
    // |addExecutionBlock| farms out the different blocks added to it. hence the
    // operations are implicitly parallelized.
    [operation addExecutionBlock:^{
      [manager performSelector:selector];
    }];
  }
  [operation setCompletionBlock:completionHandler];
  return operation;
}

- (void)addOperation:(NSOperation*)operation toQueue:(NSOperationQueue*)queue {
  DCHECK([NSThread isMainThread]);
  DCHECK(operation);
  DCHECK(queue);

  if (_lastDispatchedOperation) {
    [operation addDependency:_lastDispatchedOperation];
  }
  _lastDispatchedOperation.reset([operation retain]);
  [queue addOperation:operation];
}

@end
